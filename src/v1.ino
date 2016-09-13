/*
Hardware connections:

Any Arduino pins labeled:  SDA  SCL
Uno, Redboard, Pro:        A4   A5
Mega2560, Due:             20   21
Leonardo:                   2    3

BME280:
3.3V Sensor

TSL2561:
3.3V Sensor

*/
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <PString.h>
#include <BME280.h>
#include <SparkFunTSL2561.h>

/* Create an BME280 object, here called "BME280" */
BME280 BME280;

/* Create an SFE_TSL2561 object, here called "TSL2561" */
SFE_TSL2561 TSL2561;

/* Setting up the ethernet
ip is the ipaddress of the arduino
server is the ipaddress of the Domoticz server
*/
byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress IP(192,168,1,15);
IPAddress SERVER(192,168,1,215);
EthernetClient CLIENT;

/* IDX's of the domoticz devices
IDX_THB is "Temp + Humidity + Baro" dummydevice
IDX_W is "Wind" dummydevice
*/
const int PROGMEM IDX_THB = 29;
const int PROGMEM IDX_W = 42;

/* variables for the readings */
float temperature, humidity, pressure;

/* Sleeptime, time between sending data in milliseconds */
const unsigned long PROGMEM SLEEPTIME = 1800000; //30 minutes

/* Max message length for the PString arrays */
const byte PROGMEM MESSAGE_MAX = 120;

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
    while(!Serial) {} // Wait
  #endif

  Ethernet.begin(MAC, IP);

  while(!BME280.begin()) {
    #ifdef DEBUG
      Serial.println("BME280_init_fail");
    #endif
    sendLog("BME280_init_fail");
    delay(1000);
  }

  while(!TSL2561.begin()) {
    #ifdef DEBUG
      Serial.println("TSL2561_init_fail");
    #endif
    sendLog("TSL2561_init_fail");
    delay(1000);
  }

  #ifdef DEBUG
    Serial.println("Startup succesfull");
  #endif
  sendLog("Startup_succesfull");
}

/* Function to send the data to Domoticz */
void sendUrl(PString url) {
  char buffer[16];
  PString ipaddress(buffer, sizeof(buffer));
  ipaddress += SERVER[0];
  ipaddress += ".";
  ipaddress += SERVER[1];
  ipaddress += ".";
  ipaddress += SERVER[2];
  ipaddress += ".";
  ipaddress += SERVER[3];

  if (CLIENT.connect(SERVER, 8080)) {
    // Make a HTTP request:
    CLIENT.print(url);
    CLIENT.print(" HTTP/1.1\r\n");
    CLIENT.print("Host: ");
    CLIENT.print(ipaddress);
    CLIENT.print("\r\n");
    CLIENT.print("Accept: */*\r\n");
    CLIENT.print("User-Agent: Mozilla/4.0 (compatible; esp8266 Lua; Windows NT 5.1)\r\n");
    CLIENT.print("Authorization: Basic YWRtaW46Ym9va3NvdWJv\r\n");
    CLIENT.print("\r\n");
    CLIENT.stop();
    #ifdef DEBUG
      Serial.println("Data send succesfully");
    #endif
  } else {
    #ifdef DEBUG
      Serial.println("Connection to server failed");
    #endif
  }
}

/* Function to add message to the Domoticz log */
void sendLog(const char* message) {
  char buffer[MESSAGE_MAX];
  PString logUrl(buffer, sizeof(buffer), "GET /json.htm?type=command&param=addlogmessage&message=Weatherstation:%20"); //%20 is a space
  logUrl +=  message;
  sendUrl(logUrl);
}

/* Function to sleep Arduino */
void sleepNow() {
  unsigned long startMillis = millis();
  #ifdef DEBUG
    Serial.println("Going to sleep. Zzzzzz");
  #endif
  while (millis() - startMillis < SLEEPTIME);
}

/* Function to calculate prediction and humstat and send the data to the device */
void sendTHBData() {
  char buffer1[2];
  char buffer2[2];
  PString humstat(buffer1, sizeof(buffer1));
  PString prediction(buffer2, sizeof(buffer2));

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

  char buffer3[MESSAGE_MAX];
  PString THBUrl(buffer3, sizeof(buffer3), "GET /json.htm?type=command&param=udevice&idx=");
  THBUrl += IDX_THB;
  THBUrl += "&nvalue=0&svalue=";
  THBUrl += temperature;
  THBUrl += ";";
  THBUrl += humidity;
  THBUrl += ";";
  THBUrl += humstat;
  THBUrl += ";";
  THBUrl += pressure;
  THBUrl += ";";
  THBUrl += prediction;
  sendUrl(THBUrl);
}

bool readBME280() {
  uint8_t pressureUnit(1);  // unit: 0 = Pa, 1 = hPa, 2 = Hg, 3 = atm, 4 = bar, 5 = torr, 6 = N/m^2, 7 = psi
  BME280.ReadData(pressure, temperature, humidity, pressureUnit, true);  // Parameters: (float& pressure, float& temp, float& humidity, bool hPa = true, bool celsius = true)
  if(isnan(pressure) || isnan(temperature) || isnan(humidity)) {
    return false;
  } else {
    //readData filled the variables
    #ifdef DEBUG
      Serial.println("Pressure: " + pressure +
                      "\nTemperature: " + temperature +
                      "\nHumidity: " + humidity
                    );
    #endif
    return true;
  }
}



void loop() {
  if(readBME280()) {
    sendTHBData();
  } else {
    #ifdef DEBUG
      Serial.println("Error reading BMP180");
    #endif
    sendLog("Error_reading_BME280");
  }
  sleepNow();
}
