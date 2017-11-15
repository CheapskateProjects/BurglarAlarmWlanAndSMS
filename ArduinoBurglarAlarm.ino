/*
  Arduino burglar alarm
  
  Arduino based burglar alarm with WLAN and SMS capabilities. 
  
  ESP8266 is connected to Arduino with this code to provide WLAN capabilities (arm, disarm). 

  SIM800L module is connected to Arduino to provide SMS capabilities (arm, disarm, alert messages). 

  Siren and sensors are connected to Arduino pins and two pins are used for status LEDs. 
  
  created   Nov 2017
  by CheapskateProjects

  ---------------------------
  The MIT License (MIT)

  Copyright (c) 2017 CheapskateProjects

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>

// SMS pins
#define FONA_RX 10
#define FONA_TX 9
#define FONA_RST 8

// Message buffers
char smsBuffer[255];
char senderBuffer[255];

// Numbers for send and receive
char allowedNumbers[1][14] =
{
    // Replace with allowed phone numbers and numbers you want to alert
    "+123456789012"
};

// Fona software serial
SoftwareSerial FSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *FSerial = &FSS;

// Fona
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// Siren pins
const int sirenPin = 7;

// Reed sensors
const int sensorPin1 = 5; // Front door
const int sensorPin2 = 4; // Back door
// PIR sensors
const int sensorPin3 = 3;
const int sensorPin4 = 2;

// Status leds
// Led to show that device is initialized
const int statusPin = A0;
// Led to show that alarm is armed
const int alarmPin = A1;

void setup()
{
  // Turn siren off when booting
  pinMode(sirenPin, OUTPUT);
  digitalWrite(sirenPin, LOW);

  // Other pins should be input pins
  pinMode(sensorPin1, INPUT_PULLUP); // Pullup for switch type sensors (no need for external resistors)
  pinMode(sensorPin2, INPUT_PULLUP);
  pinMode(sensorPin3, INPUT);
  pinMode(sensorPin4, INPUT);

  // Status pins
  pinMode(statusPin, OUTPUT);
  pinMode(alarmPin, OUTPUT);
  digitalWrite(statusPin, LOW);
  digitalWrite(alarmPin, LOW);

  // Wait for serial
  Serial.begin(115200);

  // Init module serial
  Serial.println("Initializing serial connection to module");
  FSerial->begin(9600);
  if(!fona.begin(*FSerial))
  {
    Serial.println("Init ERROR: Could not find SIM module!");
    digitalWrite(alarmPin, HIGH);
    while (1);
  }

  // Initialize ESP8266 (Note that Serial1 is for Leonardo. Some Arduinos only have one serial so you should use Serial instead of Serial1 when using such device). 
  Serial1.begin ( 9600 );// Slow baud rate because I had some reliability problems with high speeds

  digitalWrite(statusPin, HIGH);
  delay(1000);
  digitalWrite(statusPin, LOW);
  
  //Give the SIM module some time so that SMS has initialized before trying to read messages
  Serial.println("Waiting for the module to fully initialize...");
  fona.type();
  delay(30000);
  fona.getNumSMS();
  delay(10000);
  Serial.println("Init done");

  // Flush ESP8266 buffer to remove init messages
  while (Serial1.available())
  {
      Serial1.read();
  }

  // Everything initialized. Set status OK
  digitalWrite(statusPin, HIGH);
}

int enabled = 0;
boolean alarmOn = false;
void loop()
{
  handleSerial();
  readMessages();
  checkAlarm();
}

void checkAlarm()
{
  if(enabled == HIGH)
  {
    if(!alarmOn) // Don't send multiple messages. When message has been sent wait for disable to reset. 
    {
      /*
       * Change this to use your custom messages for each sensor
       */
      if(digitalRead(sensorPin1) == HIGH)
      {
        alarm("Hackspace alarm: Front door");
      }
      if(digitalRead(sensorPin2) == HIGH)
      {
        alarm("Hackspace alarm: Back door");
      }
      if(digitalRead(sensorPin3) == HIGH || digitalRead(sensorPin4) == HIGH)
      {
        alarm("Hackspace alarm: Movement");
      }
    }
  }
  else
  {
    // Device has been disabled -> Turn off siren
    if(alarmOn)
    {
      digitalWrite(sirenPin, LOW);
      alarmOn = false;
    }
  }
}

void alarm(char* message)
{
  digitalWrite(sirenPin, HIGH);
  alarmOn = true;
  Serial.println("Send alarm message to:");
  int size = sizeof allowedNumbers / sizeof allowedNumbers[0];
  for(int i = 0; i < size; i = i + 1)
  {
      
      Serial.println(allowedNumbers[i]);
      fona.sendSMS(allowedNumbers[i], message);
  }
}

void readMessages()
{
  // If we have SMS messages they are handled
  int8_t SMSCount = fona.getNumSMS();
  if (SMSCount > 0)
  {
        Serial.println("Found SMS messages!");
        uint16_t smslen;
        for (int8_t smsIndex = 1 ; smsIndex <= SMSCount; smsIndex++)
        {
          Serial.print("Handling message ");
          Serial.println(smsIndex);

          // Read message and check error situations
          if (!fona.readSMS(smsIndex, smsBuffer, 250, &smslen))
          {
            Serial.println("Error: Failed to read message");
            continue;
          }
          if (smslen == 0)
          {
            Serial.println("Error: Empty message");
            continue;
          }

          // Read sender number so that we can check if it was authorized
          fona.getSMSSender(smsIndex, senderBuffer, 250);

          // Compare against each authorized number
          int size = sizeof allowedNumbers / sizeof allowedNumbers[0];
          for( int ind = 0; ind < size; ind = ind + 1)
          {
            if(strstr(senderBuffer,allowedNumbers[ind]))
            {
              // Authorized -> Handle action
              Serial.println("Allowed number found");
              if(strstr(smsBuffer, "ALARM ON"))
              {
                // Set actual state
                enabled = true;
                // Set status led
                digitalWrite(alarmPin, HIGH);
                // Set ESP8266 state
                Serial1.println("ALARM ON");
                // Debug
                Serial.println("SET ALARM ON");
                // Delete first just in case to prevent retrigger
                fona.deleteSMS(smsIndex);
                // Send confirmation message
                fona.sendSMS(allowedNumbers[ind], "Alarm enabled");
              }
              else if(strstr(smsBuffer, "ALARM OFF"))
              {
                // Set actual state
                enabled = false;
                // Set status led
                digitalWrite(alarmPin, LOW);
                // Set ESP8266 state
                Serial1.println("ALARM OFF");
                // Debug
                Serial.println("SET ALARM OFF");
                // Delete first just in case to prevent retrigger
                fona.deleteSMS(smsIndex);
                // Send confirmation message
                fona.sendSMS(allowedNumbers[ind], "Alarm disabled");
              }
              else
              {
                Serial.println("Not a known message type");
                Serial.println(smsBuffer);
              }

              // Already found authorized and did the action so we can bail out now
              break;
            }
          }

          // Message was handled or it was unauthorized -> delete
          fona.deleteSMS(smsIndex);

          // Flush buffers
          while (Serial.available())
          {
            Serial.read();
          }
          while (fona.available())
          {
            fona.read();
          }
        }
  }
}


char esp8266buffer [64];
int espIndex = 0;
void handleSerial()
{
  // Received from the main controller
  if (Serial1.available() > 0)
  {
        espIndex = 0;
        while (Serial1.available())
        {
          esp8266buffer[espIndex++] = Serial1.read();
          if (espIndex == 63)
          {
            break;
          }
          else
          {
            delay(2);
          }
        }
        esp8266buffer[espIndex] = '\0';
        Serial.print("Got me some bytes: ");
        Serial.println(espIndex);

        // Handle message here!
        if(strstr(esp8266buffer, "ALARM ON"))
        {
            enabled = true;
            digitalWrite(alarmPin, HIGH);
        }
        else if(strstr(esp8266buffer, "ALARM OFF"))
        {
            enabled = false;
            digitalWrite(alarmPin, LOW);
        }
        else
        {
          Serial.println("Unknown command");
          Serial.println(esp8266buffer);
          // Ignore unknown serial commands because we want to prevent looping error messages
        }

        // Clear buffer for next round
        for(int i = 0; i < 64; i++)
        {
          esp8266buffer[i] = ' ';
        }
        esp8266buffer[63] = '\0';
  }
}
