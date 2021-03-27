#include <Arduino.h>
#include <Wire.h>

#include "commands.hpp"
#include "commandParse.hpp"
#include "tubComm.hpp"
#include "i2cMonitor.hpp"

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    //digitalWrite(A3, 1);
    //pinMode(A3, OUTPUT);
    tubCommSetup();
    i2cMonitorSetup();
    Serial.begin(115200);
}

static void outputBool(const __FlashStringHelper* label, bool x)
{
    Serial.print(F(", \""));
    Serial.print(label);
    Serial.print(x ? F("\":true") : F("\":false"));
}

static void outputCommandResponse(const AtCommand &command, const CommandResponse &response)
{
    Serial.print(F("{ \"command\":"));
    Serial.print(static_cast<int>(command.type));
    Serial.print(F(", \"param\":"));
    Serial.print(command.param);
    Serial.print(F(", \"ret\":"));
    Serial.print(static_cast<int>(response.error));
    Serial.print(F(", \"disp\":\""));
    Serial.print(response.screenText);
    Serial.print(F("\", \"curTemp\":"));
    Serial.print(response.curTemp);
    Serial.print(F(", \"setTemp\":"));
    Serial.print(response.setTemp);
    outputBool(F("jets"), response.statJets());
    outputBool(F("light"), response.statLight());
    outputBool(F("lock"), response.statLock());
    outputBool(F("summer"), response.statSummer());
    Serial.print(F(" }\n"));
}

static inline void handleCommand(AtCommand& command)
{
    if (command.type == AT_RESET) {
        digitalWrite(A3, 0);
        delay(2000);
        digitalWrite(A3, 1);
    } else if (command.type == AT_I2CINFO) {
        i2cMonitorPrint();
    } else {
        tubCommandSend(command);
    }
}

void loop()
{
    i2cMonitorLoop();

    static AtCommand curCommand;

    // Heartbeat
    static byte x = 0;
    digitalWrite(LED_BUILTIN, x ? HIGH : LOW);
    x ^= 1;

    // Hot tub's loop is 80ms, so we'll poll a little faster
    delay(75);

    //Serial.print("Hello\n");

    // Check response; also advances command state machine
    CommandResponse &response = tubCommandResponse();

    if (response.state == RS_EMPTY)
    {
        // Command state machine is idle, ready for next command
        getNextCommand(curCommand);
        if (curCommand.type != AT_NONE) {
            handleCommand(curCommand);
        }
    }
    else if (response.state == RS_VALID)
    {
        // Command response is ready
        outputCommandResponse(curCommand, response);
        response.clear();
    }
    // TODO: Abort on timeout
}
