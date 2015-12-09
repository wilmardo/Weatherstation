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
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };    
IPAddress ip(192,168,1,15);
IPAddress server(192,168,1,215);
EthernetClient client;

/* IDX's of the domoticz devices
IDX_THB is "Temp + Humidity + Baro" dummydevice 
IDX_W is "Wind" dummydevice 
*/
const int IDX_THB = 29;
const int IDX_W = 42;

/* variables for the readings */
double temperature, humidity, pressure;

/* Sleeptime, time between sending data in milliseconds */
const unsigned long SLEEPTIME = 10000; //1800000; //30 minutes

void setup()
{
  Serial.begin(9600);
  Serial.println("REBOOT");
  
  Ethernet.begin(mac, ip);
  
  if (myBMP180.begin())
    Serial.println("BMP180 init success");
  else
  {
    Serial.println("BMP180 init fail");
    sendLog("BMP180 init fail");
    while(1); // Pause forever.
  }
  
  Serial.println("Startup succesfull");
  //sendLog("Startup succesfull");
}

/* Function to send the data to Domoticz */
void sendData(String url) {
  Serial.println(url);  
  if (client.connect(server, 8080)) {
    // Make a HTTP request:
    client.print(url + " HTTP/1.1\r\n");
    client.print("Host: " + convertIpAddress(server) + "\r\n");
    client.print("Accept: */*\r\n");
    client.print("User-Agent: Mozilla/4.0 (compatible; esp8266 Lua; Windows NT 5.1)\r\n");
    client.print("\r\n");
    Serial.println("Data send succesfully");
  } else {
    // if you didn't get a connection to the server:
    Serial.println("Connection to server failed");
  }
}

String convertIpAddress(IPAddress address)
{
 return String(address[0]) + "." + 
        String(address[1]) + "." + 
        String(address[2]) + "." + 
        String(address[3]);
}

/* Function to add message to the Domoticz log */
void sendLog(String message) {
  String URLmessage = URLEncode(message);
  String url = "GET /json.htm?type=command&param=addlogmessage&message=Weatherstation:" + URLmessage;
  sendData(url);
}

String URLEncode(String message)
{
    const char *hex = "0123456789abcdef";
    const char *msg = message.c_str();
    String encodedMsg = "";

    while (*msg!='\0'){
        if( ('a' <= *msg && *msg <= 'z')
                || ('A' <= *msg && *msg <= 'Z')
                || ('0' <= *msg && *msg <= '9') ) {
            encodedMsg += *msg;
        } else {
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

void sleepNow() {
  unsigned long startMillis = millis();
  Serial.println("Going to sleep. Zzzzzz");
  while (millis() - startMillis < SLEEPTIME);
}
  
/* Function to calculate prediction and humstat and send the data to the device */  
void sendTHBData() {
  String humstat;
  String prediction;
  
  if(humidity < 40) {
    humstat = "2";
  } else if(humidity > 70) {
    humstat =  "3";
  } else {
    humstat = "0"; 
  }
  if (pressure < 1000) {
    prediction = "4"; //Rain
  } else if (pressure < 1020){
    prediction = "3"; //Cloudy
  } else if (pressure < 1030){
    prediction = "2"; //Partly cloudy
  } else {
    prediction = "1"; //Sunny
  }

  String url = "GET /json.htm?type=command&param=udevice&idx=" + IDX_THB;
  url = url + "&nvalue=0&svalue=" + temperature + ";" + humidity + ";" + humstat + ";" + pressure + ";" + prediction;
  sendData(url);
}

boolean readDHT22() {
  DHT22_ERROR_t errorCode;
  // The sensor can only be read from every 1-2s, and requires a minimum
  // 2s warm-up after power-on.
  delay(2000);
  
  errorCode = myDHT22.readData();
  switch(errorCode)
  {
    case DHT_ERROR_NONE:
      temperature = myDHT22.getTemperatureCInt() / 10;
      humidity = myDHT22.getHumidityInt() / 10;
      return true;
    case DHT_ERROR_CHECKSUM:
      //Serial.println("check sum error);
      return false;
    case DHT_BUS_HUNG:
      //Serial.println("BUS Hung);
      return false;
    case DHT_ERROR_NOT_PRESENT:
      //Serial.println("Not Present);
      return false;
    case DHT_ERROR_ACK_TOO_LONG:
      //Serial.println("ACK time out);
      return false;
    case DHT_ERROR_SYNC_TIMEOUT:
      //Serial.println("Sync Timeout);
      return false;
    case DHT_ERROR_DATA_TIMEOUT:
      //Serial.println("Data Timeout);
      return false;
    case DHT_ERROR_TOOQUICK:
      //Serial.println("Polled to quick");
      return false;
  }
}  

boolean readBMP180()
{
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
          //Serial.println("error retrieving pressure measurement\n");
          return false;
          }
      } else {
        //Serial.println("error starting pressure measurement\n");
        return false;
        }
    } else {
      //Serial.println("error retrieving temperature measurement\n");
      return false;
      }
  } else {
    //Serial.println("error starting temperature measurement\n");
    return false;
    }
}

/* for debug purposes
  Function to print the Domoticz response to serial, to troubleshout HTTP error
*/
void printResponse() {
  while(client.available()) {
    char c = client.read();
    Serial.print(c);
  }
}

void loop()
{ 
  if(readDHT22()) {
    if(readBMP180()) {      
      sendTHBData();
    } else {
      Serial.println("Error reading BMP180");
      sendLog("Error reading BMP180");
    }
  } else {
    Serial.println("Error reading DHT22");
    sendLog("Error reading DHT22");
  }
  printResponse();
  sleepNow();
}


