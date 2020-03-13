// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#include <ESP8266WiFi.h> // connect to WiFi
#include "src/iotc/common/string_buffer.h" // IoT Central part
#include "src/iotc/iotc.h" // IoT Central part
#include <DHT.h> // DHT sensor
#include <ESP8266WebServer.h> // web-server
#include <AutoConnect.h> // WiFi autoconnect
#include <OneWire.h> // for DS18B20 sensor
#include <DallasTemperature.h> // for DS18B20 sensor

// run web-server and autoconnect app
ESP8266WebServer Server;
AutoConnect      Portal(Server);

// IOT Central credentials
const char* SCOPE_ID = "scope-id";
const char* DEVICE_ID = "device-id";
const char* DEVICE_KEY = "device-key";

// define PINs that we plan to read on ESP8266
#define DS1 5   // DS18B20 PIN 5 = D1 on ESP8266 Lolin Node MCU v3
#define DS2 4   // DS18B20 PIN 4 = D2 on ESP8266 Lolin Node MCU v3
#define DS3 0   // DS18B20 PIN 0 = D3 on ESP8266 Lolin Node MCU v3
#define DHT1 2  // DHT22   PIN 2 = D4 on ESP8266 Lolin Node MCU v3

// задаем типа устройства DHT
#define DHTTYPE DHT22

// run onewire and dht
OneWire oneWire1(DS1);
OneWire oneWire2(DS2);
OneWire oneWire3(DS3);
DHT dht1(DHT1, DHTTYPE);

// show with LED 2 that device works
#define LED_BUILTIN 2

// reading data via onewire
DallasTemperature sensor1(&oneWire1);
DallasTemperature sensor2(&oneWire2);
DallasTemperature sensor3(&oneWire3);


// initial web-page of web-server with Wi-Fi connection
void rootPage() {
  char content[] = "Hello, World!";
  Server.send(200, "text/plain", content);
}

//connect to IoT Central
void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo);
#include "src/connection.h"

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo) {
  // ConnectionStatus
  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
    LOG_VERBOSE("Is connected ? %s (%d)",
                callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO",
                callbackInfo->statusCode);
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    return;
  }

  // payload buffer doesn't have a null ending.
  // add null ending in another buffer before print
  AzureIOT::StringBuffer buffer;
  if (callbackInfo->payloadLength > 0) {
    buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
  }

  LOG_VERBOSE("- [%s] event was received. Payload => %s\n",
              callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

  if (strcmp(callbackInfo->eventName, "Command") == 0) {
    LOG_VERBOSE("- Command name was => %s\r\n", callbackInfo->tag);
  }
}

// change LED on-off delay
 void LEDOn() {
  digitalWrite(LED_BUILTIN, LOW);
  delay(2000);
}

void LEDOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

// settings
void setup() {
  Serial.begin(9600);
  Serial.println("\nReading temperature from sensors. Sensors 1, 2 and 3 are DS18B20. Sensor 4 is DHT22");
  Serial.println();
  Server.on("/", rootPage);
  if (Portal.begin()) {
    Serial.println("\nHTTP server:" + WiFi.localIP().toString());
  }

// reading data from sensors with delay
  sensor1.begin();
  delay(2000);
  sensor2.begin();
  delay(2000);
  sensor3.begin();
  delay(2000);
  dht1.begin();
  delay(2000);


// connect to IoT Central
  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  
  pinMode(LED_BUILTIN, OUTPUT);
  LEDOff();

  if (context != NULL) {
    lastTick = 0;  // set timer in the past to enable first telemetry a.s.a.p
  }
}

void loop() {
// run portal for wifi connection  
  Portal.handleClient();
  
  delay(5000);
// read temperature
// delay added to avoid errors
  sensor1.requestTemperatures();
  delay(2000);
  sensor2.requestTemperatures(); 
  delay(2000);
  sensor3.requestTemperatures(); 
  delay(2000);
  float temp4 = dht1.readTemperature(); //dht reading
  delay(2000);

  float temp1 = sensor1.getTempCByIndex(0); //ds18b20 reading
  float temp2 = sensor2.getTempCByIndex(0); //ds18b20 reading
  float temp3 = sensor3.getTempCByIndex(0); //ds18b20 reading
//  t4 = sensor3.requestTemperatures();

// sending data to IoT Central
  if (isConnected) {
    unsigned long ms = millis();

if (ms - lastTick > 180000) // sending data 1 time per 3 minutes (180000 milliseconds)
      {  
      char msg[200] = {0};
      int pos = 0, errorCode = 0; //было 
      
      lastTick = ms;
      if (loopId++ % 2 == 0) 
    
      {         // send telemetry 
          pos = snprintf(msg, sizeof(msg) -1, "{ \"house_radiators\": %4.2f, \"heating_supply_temp\": %4.2f, \"heating_return_temp\": %4.2f, \"outside_temp\": %4.2f }", temp1, temp2, temp3, temp4);
          
  
        errorCode = iotc_send_telemetry(context, msg, pos);
      } 

  
      msg[pos] = 0;

      if (errorCode != 0) {
        LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
    }

    iotc_do_work(context);  // do background work for iotc
  } else {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  }
}
