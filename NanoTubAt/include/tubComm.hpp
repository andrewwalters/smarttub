#ifndef _TUBCOMM_HPP_
#define _TUBCOMM_HPP_

#include <Arduino.h>
#include "commands.hpp"

enum ButtonCode : byte
{
    BUTTON_NONE = 0,
    BUTTON_MODE = 8,
    BUTTON_PLUS = 4,
    BUTTON_MINUS = 2,
    // Unsure if these are long presses
    // Seems to have the same effect as regular presses
    BUTTON_LONG_MODE = 0x88,
    BUTTON_LONG_PLUS = 0x84,
    BUTTON_LONG_MINUS = 0x82,
    // Not a real button code
    STATUS_REQUEST = 0xff
};

enum ResponseState : byte
{
    RS_EMPTY = 0,
    RS_STARTING,
    RS_WAITING,
    RS_CAPTURED,
    RS_VALID
};

enum ResponseError : byte
{
    RERR_NONE = 0,
    // Search for screen iterated through too many
    RERR_SCREEN_NOT_FOUND,
    // Command sequence too long
    RERR_SEQ_TOO_LONG,
    // Sequence of button events ended without setting command response
    RERR_UNEXPECTED_SEQ_END,
    // Bad command (parse error)
    RERR_BAD_COMMAND,
    // Unknown command (parsed but not implemented)
    RERR_UNKNOWN_COMMAND,
    // Temperature request failure
    RERR_TEMPERATURE_FAIL,
    // Command aborted
    RERR_ABORTED
};

struct ButtonResponse
{
    ButtonResponse() : state(RS_EMPTY), error(RERR_NONE) {}
    bool valid() const { return state == RS_VALID; }
    bool waiting() const { return state == RS_WAITING; }
    // Status helpers
    bool statSetTemp() const { return (statusBytes[0] & 0x3) == 0x3; }
    bool statCurTemp() const { return (statusBytes[0] & 0x3) == 0x1; }
    bool statJets() const { return (statusBytes[0] & 0x04); }
    bool statLight() const { return (statusBytes[0] & 0x08); }
    bool statLock() const { return (statusBytes[0] & 0x40); }
    bool statSummer() const { return (statusBytes[1] & 0x40); }
    void copyStatusFrom(const ButtonResponse& other)
    {
        memcpy(screenText, other.screenText, sizeof(screenText));
        memcpy(statusBytes, other.statusBytes, sizeof(statusBytes));
    }
    char screenText[6];
    byte statusBytes[4];
    volatile ResponseState state;
    ResponseError error;
};

struct CommandResponse : public ButtonResponse
{
    CommandResponse() : ButtonResponse() { clear(); }
    void clear()
    {
        curTemp = -1;
        setTemp = -1;
        state = RS_EMPTY;
        memset(screenText, 0, sizeof(screenText));
        memset(statusBytes, 0, sizeof(statusBytes));
    }
    int curTemp;
    int setTemp;
};

// Set up I2C, call from main setup()
void tubCommSetup();

// Process AT command, which gets mapped into one or more emulated
// button presses from remote control.
void tubCommandSend(const AtCommand &command);

// Abort a command in progress, e.g. due to timeout
void tubCommandAbort();

// Check command response. After calling tubCommandSend(), caller
// should poll on this until this indicates a valid response.
// Internally, this will also advance the state of command processing
// (e.g. checks to see if ready to send next button press)
CommandResponse &tubCommandResponse();

#endif // _TUBCOMM_HPP_
