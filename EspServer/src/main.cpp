#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>

#include "myDebug.h"
#include "serialReq.h"
#include "avrUpdater.h"
#include "realTime.h"
#include "progTimer.h"
#include "commands.hpp"

// NOTE: You must create this file from secrets_TEMPLATE.h and fill with your Wifi credentials
#include "secrets.h"

static constexpr int MIN_TEMP = 80;

//#define UPDATE_ONLY

#ifndef UPDATE_ONLY
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
SerialReq serialHandler(
    "<html>"
    "  <head></head>"
    "  <body>"
    "  <div><pre>",
    "  </pre></div>"
    "    <form action=\"http:/api\">"
    "      <div><button name=\"q\" value=\"D\">Display</button></div>"
    "      <div><button name=\"q\" value=\"M\">Mode</button></div>"
    "      <div><button name=\"q\" value=\"+\">Plus</button></div>"
    "      <div><button name=\"q\" value=\"-\">Minus</button></div>"
    "      <div><button name=\"q\" value=\"AT.STAT\">Status</button></div>"
    "      <div>"
    "        <button name=\"q\" value=\"AT.JETS1=0\">Jets 1 Off</button>"
    "        <button name=\"q\" value=\"AT.JETS1=1\">Jets 1 On</button>"
    "      </div>"
    "      <div>"
    "        <button name=\"q\" value=\"AT.JETS2=0\">Jets 2 Off</button>"
    "        <button name=\"q\" value=\"AT.JETS2=1\">Jets 2 On</button>"
    "      </div>"
    "      <div>"
    "        <button name=\"q\" value=\"AT.AIR=0\">Air Off</button>"
    "        <button name=\"q\" value=\"AT.AIR=1\">Air On</button>"
    "      </div>"
    "      <div>"
    "        <button name=\"q\" value=\"AT.TEMPADJ=-1\">Temp Down</button>"
    "        <button name=\"q\" value=\"AT.TEMPADJ=1\">Temp Up</button>"
    "      </div>"
    "    </form>"
    "  </body>"
    "</html>");
#endif

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *hostname = "smarttub";

const char *PARAM_MESSAGE = "message";

void setupWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    DPRINTF("WiFi Failed!\n");
    return;
  }

  DPRINTF("IP Address: %s\n", WiFi.localIP().toString().c_str());
}

void setupOTA()
{
  // ArduinoOTA.onStart([](){});
  // ArduinoOTA.onEnd([](){});
  // ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){});
  // ArduinoOTA.onError([](ota_error_t error){});
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
  case WS_EVT_DISCONNECT:
  case WS_EVT_ERROR:
  case WS_EVT_PONG:
  case WS_EVT_DATA:
  default:
    break;
  }
}

void setupServer()
{
#ifndef UPDATE_ONLY
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.serveStatic("/", SPIFFS, "/");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    time_t now = getTime();
    char tbuf[26];
    strcpy(tbuf, "ERR\n");
    ctime_r(&now, tbuf);
    request->send(200, "text/plain", tbuf);
  });

  // Send a GET request to <IP>/get?message=<message>
  server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("r"))
    {
      int gpio = (request->getParam("r")->value() != "0");
      pinMode(0, OUTPUT);
      digitalWrite(0, gpio);
      request->send(200, "text/plain", "OK");
    }
    else if (request->hasParam("q"))
    {
      String message = request->getParam("q")->value();
      if (serialHandler.isIdle())
      {
        serialHandler.sendRequest(message.c_str());
        AsyncWebServerResponse *response = request->beginChunkedResponse(
            "text/html", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
              size_t numBytes = maxLen;
              const char *chunk = serialHandler.getResponse(&numBytes);
              if (chunk == nullptr)
              {
                // Done
                return 0;
              }
              else if (numBytes == 0)
              {
                // Data not ready yet
                return RESPONSE_TRY_AGAIN;
              }
              else if (numBytes <= maxLen)
              {
                // Data ready
                memcpy(buffer, chunk, numBytes);
                return numBytes;
              }
              else
              {
                // HACK: Response would overflow buffer, so don't copy it
                // But also respond that data is not ready so it'll call us again,
                // so we'll be able to flush out remaining data.
                return RESPONSE_TRY_AGAIN;
              }
            });
        request->send(response);
      }
      else
      {
        request->send(503, "text/plain", "Server busy, try again later");
      }
    }
    else
    {
      request->send(400, "text/plain", "Bad request");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
#endif
  MDNS.begin(hostname);
  MDNS.addService("http", "tcp", 80);
}

static void progCheck()
{
  static bool sendingProgChange = false;
  if (sendingProgChange) {
    // Waiting for response from serial handler. Ignore response for now.
    // Needs refactored since serial handler returns entire html page.
    char buf[64];
    size_t numBytes = sizeof(buf);
    if (serialHandler.getResponse(&numBytes) == nullptr) {
      sendingProgChange = false;
    }
  } else if (serialHandler.isIdle()) {
    // Only check for program timer updates when serial handler is free
    // for sending command.
    int temperature = 0;
    bool changed = progTimer_getSetting(&temperature);
    if (changed && temperature >= MIN_TEMP) {
      char buf[64];
      // FIXME: Must keep this command in sync with definition in commands.cpp
      int sz = snprintf_P(buf, sizeof(buf), PSTR("AT.TEMPSET=%d"), temperature);
      if (sz > 0 && sz < static_cast<int>(sizeof(buf))) {
        serialHandler.sendRequest(buf);
        sendingProgChange = true;
      }
    }
  }
}

void setup()
{
  SPIFFS.begin();
  Serial.begin(115200, SERIAL_8N1, SERIAL_FULL, 1, false);
  setup_realTime();
  setupWifi();
  setupOTA();
  avrUpdater_start();
  setupServer();
  progTimer_setup();
}

void loop()
{
  ArduinoOTA.handle();
  ws.cleanupClients();
  avrUpdater_loop();
  progCheck();
}