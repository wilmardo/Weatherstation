/* 
Hardware connections:

BMP180:
3.3V sensor
Any Arduino pins labeled:  SDA  SCL
Uno, Redboard, Pro:        A4   A5
Mega2560, Due:             20   21
Leonardo:                   2    3

DHT22:
3.3V sensor
PIN is set by the DHT22_PIN variable. In this example pin 7
*/

#include <SPI.h>
#include <Ethernet.h>
#include <SFE_BMP180.h>
#include <DHT22.h>
#include <Wire.h>

#define DHT22_PIN 7

/* Create an SFE_BMP180 object, here called "myBMP180" */
SFE_BMP180 myBMP180;

/* Create a DHT22 object, here called "myDHT22" */
DHT22 myDHT22(DHT22_PIN);

/* Setting up the ethernet
ip is the ipaddress of the arduino
server is the ipaddress of the Domoticz server
*/
byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };    
const IPAddress PROGMEM IP(192,168,1,15);
const IPAddress PROGMEM SERVER(192,168,1,215);
EthernetClient CLIENT;

/* IDX's of the domoticz devices
IDX_THB is "Temp + Humidity + Baro" dummydevice 
IDX_W is "Wind" dummydevice 
*/
const byte PROGMEM IDX_THB = 29;
const byte PROGMEM IDX_W = 42;

/* variables for the readings */
double temperature, humidity, pressure;

/* Sleeptime, time between sending data in milliseconds */
const unsigned long PROGMEM SLEEPTIME = 10000; //900000; //15 minutes

/* Max message length for the arrays */
const byte PROGMEM MESSAGE_MAX = 120;

void setup()
{
	Serial.begin(9600);
	Serial.println("REBOOT");
  
	Ethernet.begin(MAC, IP);
  
	if (myBMP180.begin()) {
		Serial.println("BMP180 init success");
	} else {
		Serial.println("BMP180 init fail");
		sendLog("BMP180 init fail");
		while(1); // Pause forever.
	}

  Serial.println(byteToChar(IDX_THB));
	Serial.println("Startup succesfull");
	sendLog("Startup succesfull");
}

/* Function to convert float to char */ 
char* floatToChar(float input) {
	char* output;
	dtostrf(input, 4, 2, output); 
	return output;
}

/* Function to convert byte to char */ 
char* byteToChar(byte input) {
	char* output;
	snprintf(output, 3, "%d", input);
	return output;
}

void ipAddressToChar(char *combined) {
	byte first_octet = IP[0];
	byte second_octet = IP[1];
	byte third_octet = IP[2];
	byte fourth_octet = IP[3];
  
	strcat(combined, byteToChar(first_octet));
	strcat(combined, byteToChar(second_octet));
	strcat(combined, byteToChar(third_octet));
	strcat(combined, byteToChar(fourth_octet));
}

/* Function to Encode URL to RFC3968 standard
http://hardwarefun.com/tutorials/url-encoding-in-arduino
*/
void urlEncode(char *encodedMsg)
{
	char msg;
	const char *hex = "0123456789abcdef";
	int count;

	while( (msg = getchar()) != EOF ){
		if( ('a' <= msg && msg <= 'z') || ('A' <= msg && msg <= 'Z') || ('0' <= msg && msg <= '9') ) {
			encodedMsg[count] = msg;
			count++;
		} else {
			encodedMsg[count] = '%';
			count++;
			encodedMsg[count] = hex[msg >> 4];
			count++;
			encodedMsg[count] = hex[msg & 15];
			count++;
			}
	}
}

/* Function to send the data to Domoticz */
void sendData(char* url) {
	char ipAddressArray[15];
	ipAddressToChar(ipAddressArray);

  char encodedMsg[MESSAGE_MAX];
  urlEncode(encodedMsg);

	if (CLIENT.connect(SERVER, 8080)) {
		// Make a HTTP request:
		CLIENT.print(encodedMsg);
		CLIENT.print(F(" HTTP/1.1\r\n"));
		CLIENT.print(F("Host: "));
		CLIENT.print(ipAddressArray);
		CLIENT.print(F("\r\n"));
		CLIENT.print(F("Accept: */*\r\n"));
		CLIENT.print(F("User-Agent: Mozilla/4.0 (compatible; esp8266 Lua; Windows NT 5.1)\r\n"));
		CLIENT.print(F("\r\n"));
		Serial.println(F("Data send succesfully"));
	} else {
		// if you didn't get a connection to the server:
		Serial.println("Connection to server failed");
	}
}

/* Function to calculate prediction and humstat and send the data to the device */  
void sendTHBData() {
	byte humstat;
	byte prediction;
  
	if(humidity < 40) {
		humstat = 2;
	} else if(humidity > 70) {
		humstat =  3;
	} else {
		humstat = 0; 
	}
	if (pressure < 1000) {
		prediction = 4; //Rain
	} else if (pressure < 1020){
		prediction = 3; //Cloudy
	} else if (pressure < 1030){
		prediction = 2; //Partly cloudy
	} else {
		prediction = 1; //Sunny
	}

	char *url = "GET /json.htm?type=command&param=udevice&idx=";
	char combined[MESSAGE_MAX];
  
	Serial.println(url);

	strcat(combined, url);
	strcat(combined, byteToChar(IDX_THB));
	strcat(combined, "&nvalue=0&svalue=");
	strcat(combined, floatToChar(temperature));
	strcat(combined, ";");
	strcat(combined, floatToChar(humidity));
	strcat(combined, ";");
	strcat(combined, byteToChar(humstat));
	strcat(combined, ";");
	strcat(combined, floatToChar(pressure));
	strcat(combined, ";");
	strcat(combined, byteToChar(prediction));
  
	sendData(combined);
}

/* Function to add message to the Domoticz log */
void sendLog(char* message) {
	char *url = "GET /json.htm?type=command&param=addlogmessage&message=Weatherstation: ";
	char combined[MESSAGE_MAX];
  
	strcat(combined, url);
	strcat(combined, message);
	sendData(combined);
}

/* Function to sleep Arduino for SLEEPTIME ammount */
void sleepNow() {
	unsigned long startMillis = millis();
	Serial.println(F("Going to sleep. Zzzzzz"));
	while (millis() - startMillis < SLEEPTIME);
}

/* for debug purposes
Function to print the Domoticz response to serial, to troubleshout HTTP error
*/
void printResponse() {
	while(CLIENT.available()) {
		char c = CLIENT.read();
		Serial.print(c);
	}
}

/* Function to read DHT22 sensor 
https://github.com/nethoncho/Arduino-DHT22
*/
boolean readDHT22() {
	DHT22_ERROR_t errorCode;
	// The sensor can only be read from every 1-2s, and requires a minimum
	// 2s warm-up after power-on.
	delay(2000);
  
	errorCode = myDHT22.readData();
	switch(errorCode) {
	case DHT_ERROR_NONE:
		temperature = myDHT22.getTemperatureCInt() / 10;
		humidity = myDHT22.getHumidityInt() / 10;
		return true;
	case DHT_ERROR_CHECKSUM:
		//Serial.println(F("check sum error));
		return false;
	case DHT_BUS_HUNG:
		//Serial.println(F("BUS Hung));
		return false;
	case DHT_ERROR_NOT_PRESENT:
		//Serial.println(F("Not Present));
		return false;
	case DHT_ERROR_ACK_TOO_LONG:
		//Serial.println(F("ACK time out));
		return false;
	case DHT_ERROR_SYNC_TIMEOUT:
		//Serial.println(F("Sync Timeout));
		return false;
	case DHT_ERROR_DATA_TIMEOUT:
		//Serial.println(F("Data Timeout));
		return false;
	case DHT_ERROR_TOOQUICK:
		//Serial.println(F("Polled to quick"));
		return false;
	}
}  

/* Function to read data from the BMP180 sensor
https://github.com/sparkfun/BMP180_Breakout_Arduino_Library/
*/
boolean readBMP180() {
	char status;

	// You must first get a temperature measurement to perform a pressure reading.
  
	// Start a temperature measurement:
	// If request is successful, the number of ms to wait is returned.
	// If request is unsuccessful, 0 is returned.

	status = myBMP180.startTemperature();
	if (status != 0) {
	// Wait for the measurement to complete:

	delay(status);

	// Retrieve the completed temperature measurement:
	// Note that the measurement is stored in the variable T.
	// Use '&T' to provide the address of T to the function.
	// Function returns 1 if successful, 0 if failure.

	status = myBMP180.getTemperature(temperature);
	if (status != 0) {
		// Start a pressure measurement:
		// The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
		// If request is successful, the number of ms to wait is returned.
		// If request is unsuccessful, 0 is returned.

		status = myBMP180.startPressure(3);
		if (status != 0) {
		// Wait for the measurement to complete:
		delay(status);

		// Retrieve the completed pressure measurement:
		// Note that the measurement is stored in the variable P.
		// Use '&P' to provide the address of P.
		// Note also that the function requires the previous temperature measurement (T).
		// (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
		// Function returns 1 if successful, 0 if failure.

		status = myBMP180.getPressure(pressure,temperature);
		if (status != 0){
			return true;
		} else {
			//Serial.println(F("error retrieving pressure measurement\n"));
			return false;
			}
		} else {
		//Serial.println(F("error starting pressure measurement\n"));
		return false;
		}
	} else {
		//Serial.println(F("error retrieving temperature measurement\n"));
		return false;
		}
	} else {
	//Serial.println(F("error starting temperature measurement\n"));
	return false;
	}
}

void loop()
{ 
	if(readDHT22()) {
		if(readBMP180()) {     
			sendTHBData();
		} else {
			Serial.println(F("Error reading BMP180"));
			sendLog("Error reading BMP180");
		}
	} else {
		Serial.println(F("Error reading DHT22"));
		sendLog("Error reading DHT22");
	}

	printResponse();
	sleepNow();
}



