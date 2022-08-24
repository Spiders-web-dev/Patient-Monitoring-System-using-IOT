#include <ESP8266WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

#define MAX_BRIGHTNESS 255
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data


int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid




char const * SSID_NAME = "iPhone"; // Put here your SSID name
char const * SSID_PASS = "Miraz321"; // Put here your password
char const * TOKEN = "BBFF-6cRKEckqeZaWaceMZDApts1t2oM67G"; // Put here your TOKEN

char const * HTTPSERVER = "things.ubidots.com";

char const * DEVICE_LABEL = "max30102"; // Put here your device label

/* Put here your variable's labels*/
char const * VARIABLE_LABEL_1 = "BPM";
char const * VARIABLE_LABEL_2 = "Oxyzen";
char const * VARIABLE_LABEL_3 = "var-test-3";
char const * VARIABLE_LABEL_4 = "var-test-4";
char const * VARIABLE_LABEL_5 = "var-test-5";
char const * VARIABLE_LABEL_6 = "var-test-6";

const int HTTPPORT = 80;

char const * USER_AGENT = "ESP8266";
char const * VERSION = "1.0";
char payload[700];
char topic[1000];

WiFiClient clientUbi;

// Space to store values to send
char str_val_1[30];
char str_val_2[30];
char str_val_3[30];
char str_val_4[30];
char str_val_5[30];
char str_val_6[30];

/********************************
 * Auxiliar Functions
 *******************************/

void SendToUbidots(char* payload) {

  int i = strlen(payload); 
  /* Builds the request POST - Please reference this link to know all the request's structures https://ubidots.com/docs/api/ */
  sprintf(topic, "POST /api/v1.6/devices/%s/?force=true HTTP/1.1\r\n", DEVICE_LABEL);
  sprintf(topic, "%sHost: things.ubidots.com\r\n", topic);
  sprintf(topic, "%sUser-Agent: %s/%s\r\n", topic, USER_AGENT, VERSION);
  sprintf(topic, "%sX-Auth-Token: %s\r\n", topic, TOKEN);
  sprintf(topic, "%sConnection: close\r\n", topic);
  sprintf(topic, "%sContent-Type: application/json\r\n", topic);
  sprintf(topic, "%sContent-Length: %d\r\n\r\n", topic, i);
  sprintf(topic, "%s%s\r\n", topic, payload);

  /* Connecting the client */
  clientUbi.connect(HTTPSERVER, HTTPPORT); 

  if (clientUbi.connected()) {
    /* Sends the request to the client */
    clientUbi.print(topic);
    Serial.println("Connected to Ubidots - POST");
  } else {
    Serial.println("Connection Failed ubidots - Try Again"); 
  }  
    
  /* Reads the response from the server */
  while (clientUbi.available()) {
    char c = clientUbi.read();
    //Serial.print(c); // Uncomment this line to visualize the response on the Serial Monitor
  }

  /* Disconnecting the client */
  clientUbi.stop();
}

/********************************
 * Main Functions
 *******************************/

void setup() {
  Serial.begin(115200);
  
  /* Connects to AP */
  WiFi.begin(SSID_NAME, SSID_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }

  WiFi.setAutoReconnect(true);
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());



  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30102 was not found. Please check wiring/power."));
    while (1);
  }

  Serial.println(F("Attach sensor to finger with rubber band. Press any key to start conversion"));

  Serial.read();

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
}

void loop() {




   bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample

    Serial.print(F("red="));
    Serial.print(redBuffer[i], DEC);
    Serial.print(F(", ir="));
    Serial.println(irBuffer[i], DEC);
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  while (1)
  {
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++)
    {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }

    //take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data


      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample

      //send samples and calculation result to terminal program through UART
      Serial.print(F("red="));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", ir="));
      Serial.print(irBuffer[i], DEC);

      Serial.print(F(", HR="));
      Serial.print(heartRate, DEC);

      Serial.print(F(", HRvalid="));
      Serial.print(validHeartRate, DEC);

      Serial.print(F(", SPO2="));
      Serial.print(spo2, DEC);

      Serial.print(F(", SPO2Valid="));
      Serial.println(validSPO2, DEC);
      delay(3000);


  
  float sensor_value_1 =heartRate;
  float sensor_value_2 =spo2;
  float sensor_value_3 = random(0, 1000)*1.0;
  float sensor_value_4 = random(0, 1000)*1.0;
  float sensor_value_5 = random(0, 1000)*1.0;
  float sensor_value_6 = random(0, 1000)*1.0;

  /*---- Transforms the values of the sensors to char type -----*/

  /* 4 is mininum width, 2 is precision; float value is copied onto str_val*/
  dtostrf(sensor_value_1, 4, 2, str_val_1);
  dtostrf(sensor_value_2, 4, 2, str_val_2);
  dtostrf(sensor_value_3, 4, 2, str_val_3);
  dtostrf(sensor_value_4, 4, 2, str_val_4);
  dtostrf(sensor_value_5, 4, 2, str_val_5);
  dtostrf(sensor_value_6, 4, 2, str_val_6);
  
  /* Builds the payload with structure: {"temperature":25.00,"humidity":50.00} for first 4 variables*/

  // Important: Avoid to send a very long char as it is very memory space costly, send small char arrays
  sprintf(payload, "{\"");
  sprintf(payload, "%s%s\":%s", payload, VARIABLE_LABEL_1, str_val_1);
  sprintf(payload, "%s,\"%s\":%s", payload, VARIABLE_LABEL_2, str_val_2);
  sprintf(payload, "%s,\"%s\":%s", payload, VARIABLE_LABEL_3, str_val_3);
  sprintf(payload, "%s,\"%s\":%s", payload, VARIABLE_LABEL_4, str_val_4);
  sprintf(payload, "%s}", payload);
  Serial.println(payload);

  /* Calls the Ubidots Function POST */
  SendToUbidots(payload);

  /* Builds the payload with structure: {"temperature":25.00,"humidity":50.00} for the rest of variables*/
  sprintf(payload, "%s", ""); //Cleans the content of the payload
  sprintf(payload, "{\"");
  sprintf(payload, "%s%s\":%s", payload, VARIABLE_LABEL_5, str_val_5);
  sprintf(payload, "%s,\"%s\":%s", payload, VARIABLE_LABEL_6, str_val_6);
  sprintf(payload, "%s}", payload);

  /* Calls the Ubidots Function POST */
  SendToUbidots(payload);
  
  delay(3000);


    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  }

}
