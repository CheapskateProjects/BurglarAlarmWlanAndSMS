/*
  ESP8266AlarmControl

  WLAN controller for the burglar alarm. This controller can arm or disarm the device through WLAN. 

  Serial is used to communicate with main module to arm or disarm the module.

  created   Nov 2017
  by CheapskateProjects

  ---------------------------
  The MIT License (MIT)

  Copyright (c) 2017 CheapskateProjects

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// WLAN configuration
const char* ssid = "XXX";
const char* password = "YYY";

// status 
bool armed = false;

ESP8266WebServer server(80);

// Function for handling the actual pin status changes.
void toggle()
{
  if(armed)
  {
    Serial.println("ALARM OFF"); // Sent to main controller
    armed = false;
  }
  else
  {
    Serial.println("ALARM ON"); // Sent to main controller
    armed = true;
  }
  
  server.send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"0; url=/\" /></head><body></body></html>");
}

// Page content
void handlePrint()
{
  String html = "<html><head><meta http-equiv=\"refresh\" content=\"30; url=/\" /></head><body style=\"margin:10px;\"><a style=\"display: inline-block; height: 30%; background-color: grey; color: white; font-size: 25vmin; font-family: arial; width: 100%; text-align:center; text-decoration: none;\" href=\"/toggle\">";

  if(armed)
  {
    html += "Disarm";
  }
  else
  {
    html += "Arm";
  }

  html += "</a></body></html>";
  
  server.send(200, "text/html", html);
}

// Init wlan connection
void initWLAN()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
}

void setup(void)
{
  Serial.begin ( 9600 );// Slow baud rate because I had some reliability problems with high speeds
  initWLAN();

  // Print these even though main controller can not handle this information. Anyhow, it will be needed for debugging. 
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );
  
  server.on("/toggle", [](){toggle();});
  server.onNotFound(handlePrint);
  server.begin();
}

char inBuffer [64];
int bufferIndex = 0;
void handleSerial()
{
  // Received from the main controller
  if (Serial.available() > 0)
  {
        bufferIndex = 0;
        while (Serial.available())
        {
          inBuffer[bufferIndex++] = Serial.read();
          
          if ((inBuffer[bufferIndex-1] == '\n') || (bufferIndex == sizeof(inBuffer)-1))
          {
            inBuffer[bufferIndex] = '\0';
            break;
          }
          
          delay(2);
        }

        // Handle message here!
        if(strstr(inBuffer, "ALARM ON"))
        {
            armed = true;
        }
        else if(strstr(inBuffer, "ALARM OFF"))
        {
            armed = false;
        }
        else
        {
          // Ignore unknown serial commands because we want to prevent looping error messages
        }

        // Clear buffer for next round
        for(int i = 0; i < 64; i++)
        {
          inBuffer[i] = ' ';
        }
        inBuffer[63] = '\0';
  }
}

void loop(void)
{
  server.handleClient();
  handleSerial();
}
