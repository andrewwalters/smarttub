// Reading and parsing AT commands from serial port

#include <Arduino.h>
#include "commands.hpp"

static const size_t kCmdBufSize = 80;
static char cmdBuf[kCmdBufSize];
static size_t cmdBufLen = 0;

static void parseCommand(AtCommand& command)
{
    // Just a newline
    if (cmdBufLen == 0) {
        command.type = AT_NONE;
        return;
    }

    // Parse possible command parameter, format AT<cmd>=<n>
    int param = 0;
    for (size_t i = 0; i < (cmdBufLen-1); i++) {
        if (cmdBuf[i] == '=') {
            // Truncate parameter string, leaving just command
            cmdBuf[i++] = '\0';
            bool neg = false;
            if (cmdBuf[i] == '-') {
                neg = true;
                i++;
            }
            for (; i < cmdBufLen; i++) {
                if (isdigit(cmdBuf[i])) {
                    param = 10 * param + (cmdBuf[i]-'0');
                } else {
                    command.type = AT_ERROR;
                    return;
                }
            }
            if (neg) {
                param = -param;
            }
        }
    }
    command.param = param;

    // Command lookup
    for (size_t i = 0; i < sizeof(atCmdList)/sizeof(atCmdList[0]); i++) {
        if (0 == strcmp_P(cmdBuf, atCmdList[i])) {
            command.type = static_cast<AtCommandType>(i);
            return;
        }
    }

    // Command not found
    command.type = AT_ERROR;
}

void getNextCommand(AtCommand& command)
{
    command.type = AT_NONE;
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (cmdBufLen < kCmdBufSize) {
                cmdBuf[cmdBufLen] = '\0';
                parseCommand(command);
                cmdBufLen = 0;
            } else {
                command.type = AT_ERROR;
            }
            break;
        }
        if (cmdBufLen < kCmdBufSize) {
            cmdBuf[cmdBufLen++] = toupper(c);
        }
    }
}