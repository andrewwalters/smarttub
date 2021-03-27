// I2C communication with hot tub

#include <Arduino.h>
#include <Wire.h>

#include "tubComm.hpp"
#include "commands.hpp"

static constexpr int I2C_ADDRESS = 0x18;

static volatile ButtonCode currentButton = ButtonCode::BUTTON_NONE;
static unsigned long buttonTime = 0;
static bool respRead = false;
static byte respData = 0x01;
static byte respAck = 0x01;

static ButtonResponse buttonResponse;
static void captureButtonResponse(const byte *buf, size_t len);
static void parseButtonResponse();

// Callback for processing I2C data sent to remote's address. Sent data
// indicates transaction type ("read" or "write") and format of expected response.
// See below for detail.
static void receiveEvent(int howMany)
{
    static constexpr size_t tmpSize = 16;
    static byte tmp[tmpSize];
    size_t num = 0;
    // Checksum for write transactions
    byte check = I2C_ADDRESS << 1;

    while (Wire.available() && num < tmpSize)
    {
        byte x = Wire.read();
        check ^= x;
        tmp[num++] = x;
    }

    if (num > 3 && check == 0 && tmp[0] == 0)
    {
        // Write transaction.
        // Amount of data received is more than 3 bytes. Format:
        //  00 <addr> <len> <len bytes> <checksum>
        // Where checksum is xor of shifted address and all data bytes.
        // Expected write response always 2 bytes:
        //  <ack> <ack>
        // Where ack byte varies (see below)
        respRead = false;

        if (tmp[1] == 0x2b && tmp[2] == 9 && num >= 12)
        {
            // 00 2b 09 - Recevies status for display
            // Leave respAck alone. It was set according to last status request read (00 2a 01)
            // Button request fully handled, ready for next
            if (buttonResponse.state == RS_WAITING)
            {
                captureButtonResponse(tmp, num);
            }
        }
        else if (tmp[1] == 0x2a && tmp[2] == 1)
        {
            // 00 2a 01 (90) - Power on sync (always writes 0x90 and response 0x81?)
            respAck = 0x81;
        }
        else
        {
            // 00 00 01 (50) - Always writes 0x50-- fw version?
            // Also use this for unknown write requests
            respAck = 0x01;
        }
    }
    else if (num == 3)
    {
        // Read transaction
        // Exactly 3 bytes received, format:
        //  00 <addr> <len>
        // Expected response:
        //  <ack> <len bytes> <checksum>
        // Where checksum is xor of ack and data bytes. For remote reads,
        // requested length is always just a single byte.
        respRead = true;

        if (tmp[1] == 0x2a && tmp[2] == 1)
        {
            // 00 2a 01 - Regular status request
            if (currentButton == ButtonCode::BUTTON_NONE)
            {
                // Don't need anything
                respAck = 0x01;
            }
            else if (currentButton == ButtonCode::STATUS_REQUEST)
            {
                // Request status but no button pressed
                respAck = 0x09;
                if (buttonResponse.state == RS_STARTING) {
                    buttonResponse.state = RS_WAITING;
                }
            }
            else
            {
                // Button pressed
                respAck = 0x19;
            }
            respData = respAck; // data == ack for this type of read
        }
        else if (tmp[1] == 0x34 && tmp[2] == 1)
        {
            // 00 34 01 - Request for which button pressed
            // Leave respAck alone; it was set to 0x19 above
            // Button press type encoded in state for data
            respData = static_cast<byte>(currentButton);
            if (buttonResponse.state == RS_STARTING) {
                buttonResponse.state = RS_WAITING;
            }
        }
        else
        {
            // Unknown, default
            respAck = 0x01;
            respData = 0x01;
        }
    }
}

// Callback for I2C data requested from remote's address
static void requestEvent()
{
    if (respRead)
    {
        // Read response is 3 bytes (since 1 byte of data is always requested)
        Wire.write(respAck);
        Wire.write(respData);
        Wire.write(respAck ^ respData); // checksum
    }
    else
    {
        // Write response always 2 ack bytes
        Wire.write(respAck);
        Wire.write(respAck);
    }
}

// Do only low-level capture of button response here since it's called from ISR
static void captureButtonResponse(const byte *buf, size_t len)
{
    if (len < 12)
    {
        return;
    }

    // First 5 bytes are ASCII text of what remote displays
    memcpy(&buttonResponse.screenText, &buf[3], 5);
    buttonResponse.screenText[5] = '\0';
    // Next 4 bytes are status bytes with bits indicating status
    memcpy(&buttonResponse.statusBytes, &buf[8], 4);
    buttonResponse.state = RS_CAPTURED;

    currentButton = ButtonCode::BUTTON_NONE;
}

static void abortButtonResponse()
{
    currentButton = BUTTON_NONE;
    buttonResponse.error = RERR_ABORTED;
    buttonResponse.state = RS_VALID;
}

static void printHex(uint8_t val)
{
    Serial.print(val >> 4, 16);
    Serial.print(val & 0xf, 16);
}

// Higher level parsing when we actually want to return state
static void parseButtonResponse()
{
    if (buttonResponse.state == RS_CAPTURED)
    {
        Serial.print(buttonResponse.screenText);
        Serial.print(' ');
        for (uint8_t i = 0; i < sizeof(buttonResponse.screenText)-1; i++) {
            printHex(buttonResponse.screenText[i]);
        }
        Serial.print(' ');
        for (uint8_t i = 0; i < sizeof(buttonResponse.statusBytes); i++) {
            printHex(buttonResponse.statusBytes[i]);
        }
        Serial.print('\n');
        //Serial.print(F("Got "));
        //Serial.println(static_cast<int>(buttonResponse.error));
        // Do any additional necessary processing here
        buttonResponse.state = RS_VALID;
    }
    else if (buttonResponse.state == RS_STARTING || buttonResponse.state == RS_WAITING)
    {
        //Serial.print(F("Wait "));
        //Serial.println(static_cast<int>(buttonResponse.state));
        // Waiting for response, check against timeout
        unsigned long delta = millis() - buttonTime;
        if (delta > 500)
        {
            abortButtonResponse();
        }
    }
}

// Call from main setup()
void tubCommSetup()
{
    Wire.begin(I2C_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}

// Should only call when there is no outstanding button
static void tubCommButtonPress(ButtonCode code)
{
    Serial.print(F("press "));
    Serial.println(static_cast<int>(code));
    buttonTime = millis();
    currentButton = code;
    buttonResponse.state = RS_STARTING;
    buttonResponse.error = RERR_NONE;
}

static CommandResponse commandResponse;

// Sequence of button functions, passes in button press and handler callback,
// which can be chained to a subsequent seqButton function.

typedef void (*handlerFn)();
static handlerFn currentHandler = nullptr;

static void seqButton(ButtonCode code, handlerFn handler)
{
    currentHandler = handler;
    tubCommButtonPress(code);
}

static void seqDone()
{
    currentHandler = nullptr;
    commandResponse.state = RS_VALID;
}

static void seqDoneIfNotSet()
{
    if (!currentHandler)
    {
        commandResponse.state = RS_VALID;
    }
}

// String match with simple wildcards
// - Strips leading and following spaces from text to test
// - Trailing '+' and '-' are ignored
// - Case-insensitive compare
// - '?' matches any single character
// - Lengths must match after stripping spaces
// Returns 0 if matched, nonzero if not
static int wcMatch(const char *cmp, const char *txt)
{
    while (*txt == ' ')
    {
        txt++;
    }
    while (*cmp != '\0' && *txt != '\0')
    {
        if (*cmp != toupper(*txt) && *cmp != '?')
        {
            return 1;
        }
        cmp++;
        txt++;
    }
    while (*txt == ' ' || *txt == '-' || *txt == '+')
    {
        txt++;
    }
    // At least one will be '\0' from while above; other should be too
    return (*cmp != *txt);
}

// Returns numerical temperature if txt matches, else -1
//  'nnnF' where 'nnn' is a digit sequence, or
//  'UT-x' where 'x' is a single digit and temp is 104+x
static int parseTemp(const char *txt)
{
    // Skip leading space
    while (*txt == ' ')
    {
        txt++;
    }
    // Handle UT-x
    if (toupper(txt[0]) == 'U' && toupper(txt[1]) == 'T' && txt[2] == '-')
    {
        char d = txt[3];
        if (isdigit(d))
        {
            return 104 + (d - '0');
        }
        else
        {
            return -1;
        }
    }
    // Handle numerical temp
    int temp = 0;
    while (isdigit(*txt))
    {
        temp = 10 * temp + (*txt - '0');
        txt++;
    }
    // Trailing 'F'
    if (toupper(*txt++) != 'F')
    {
        return -1;
    }
    // Trailing spaces
    while (*txt == ' ')
    {
        txt++;
    }
    return (*txt == '\0') ? temp : -1;
}

// Prefer to use lambda with capture instead of static value, but can't
// do that without pulling in a bunch of C++ stdlib stuff
// (Could also stick into a "static class" just for encapsulation)
static constexpr int kMaxScreenSearch = 20;
static int screenSearchCount = 0;
// Hacky way to specify special screens-- content doesn't matter, only the pointer
static const char *const TEMPDISP_SCREEN = "\t";
static const char *const TEMPADJ_SCREEN = "\a";
static const char *searchScreen = "";
static handlerFn handlerAfterSearch = nullptr;

static void searchScreenHandler()
{
    if ((((searchScreen == TEMPADJ_SCREEN && buttonResponse.statSetTemp()) ||
          (searchScreen == TEMPDISP_SCREEN && buttonResponse.statCurTemp())) && parseTemp(buttonResponse.screenText) >= 0) ||
        0 == wcMatch(searchScreen, buttonResponse.screenText))
    {
        if (handlerAfterSearch)
        {
            // Found our screen, call next handler
            handlerAfterSearch();
        }
    }
    else
    {
        // Not found
        if (++screenSearchCount > kMaxScreenSearch)
        {
            // Searched through too many screens
            // Bail out so we don't search forever
            commandResponse.state = RS_VALID;
            commandResponse.error = RERR_SCREEN_NOT_FOUND;
            seqDone();
        }
        else
        {
            // Press button to next screen
            ButtonCode code = BUTTON_MODE;
            if (0 == wcMatch("TOOLS", buttonResponse.screenText) || 0 == wcMatch("EXIT", buttonResponse.screenText))
            {
                // Need '+' to cycle through tools menu and to exit
                code = BUTTON_PLUS;
            }
            // Send next button press and handle it here
            seqButton(code, searchScreenHandler);
        }
    }
}
void seqSearchScreen(const char *screen, handlerFn handler)
{
    // Note: capturing just pointer, not copying string
    // Ok for constant strings, not for locally defined or changing strings
    screenSearchCount = 0;
    searchScreen = screen;
    handlerAfterSearch = handler;
    seqButton(STATUS_REQUEST, searchScreenHandler);
}

// Single '+' button then done
static void seqPlus()
{
    seqButton(ButtonCode::BUTTON_PLUS, seqDone);
}

// Single '-' button then done
static void seqMinus()
{
    seqButton(ButtonCode::BUTTON_MINUS, seqDone);
}

// Search for screen, then hit plus or minus button
// '+' if plusMinus > 0, '-' if <= 0
static void seqScreenPlusMinus(const char *screenName, int plusMinus)
{
    seqSearchScreen(screenName, (plusMinus > 0) ? seqPlus : seqMinus);
}

static void seqSetLight(int value)
{
    // Button sequences for each light level.
    // Sequences are worst case since we don't know current state of light.
    // Button behavior (showing light state transitions):
    //  + : 0->3, 1->2, 2->3, 3->3 (2 plusses from any state gets us to 3)
    //  - : 0->0, 1->0, 2->1, 3->2 (3 minuses from any state gets us to 0)
    const static ButtonCode buttons[4][5] = {
        {BUTTON_MINUS, BUTTON_MINUS, BUTTON_MINUS, BUTTON_NONE},
        {BUTTON_PLUS, BUTTON_PLUS, BUTTON_MINUS, BUTTON_MINUS, BUTTON_NONE},
        {BUTTON_PLUS, BUTTON_PLUS, BUTTON_MINUS, BUTTON_NONE},
        {BUTTON_PLUS, BUTTON_PLUS, BUTTON_NONE}};
    static int curButton;
    static int buttonIndex;
    curButton = 0;
    buttonIndex = (value < 0) ? 0 : ((value > 3) ? 3 : value);

    static handlerFn nextButton = []() {
        if (buttons[buttonIndex][curButton] == BUTTON_NONE)
        {
            seqDone();
        }
        else
        {
            seqButton(buttons[buttonIndex][curButton], nextButton);
            curButton++;
        }
    };
    seqSearchScreen("LITE", nextButton);
}

static void seqSetTemperature(int value)
{
    static int setTemp;

    setTemp = value;

    static handlerFn nextButton = []() {
        int curTemp = parseTemp(buttonResponse.screenText);
        if (curTemp < 0)
        {
            // Temperature unreadable
            commandResponse.error = RERR_TEMPERATURE_FAIL;
            seqDone();
        }
        else if (curTemp == setTemp)
        {
            // Hit our temperature
            seqDone();
        } 
        else
        {
            seqButton((setTemp > curTemp) ? BUTTON_PLUS : BUTTON_MINUS, nextButton);
        }
    };
    seqSearchScreen(TEMPADJ_SCREEN, nextButton);
}

static void seqFullStat()
{
    static byte attempts = 0;
    static constexpr byte MAX_ATTEMPTS = 2;
    static handlerFn readTemp = []() {
        attempts++;
        int temp = parseTemp(buttonResponse.screenText);
        if (buttonResponse.statCurTemp())
        {
            commandResponse.copyStatusFrom(buttonResponse);
            commandResponse.curTemp = temp;
            // Hit minus button to try to switch to set temperature
            if (commandResponse.setTemp < 0 && attempts <= MAX_ATTEMPTS)
            {
                seqSearchScreen(TEMPADJ_SCREEN, readTemp);
            }
        }
        else if (buttonResponse.statSetTemp())
        {
            commandResponse.copyStatusFrom(buttonResponse);
            commandResponse.setTemp = temp;
            if (commandResponse.curTemp < 0 && attempts <= MAX_ATTEMPTS)
            {
                // Cycle around through everything to try to get to current temp
                seqButton(ButtonCode::BUTTON_MODE, []() {
                    seqSearchScreen(TEMPDISP_SCREEN, readTemp);
                });
            }
        }
        seqDoneIfNotSet();
    };
    attempts = 0;
    seqSearchScreen(TEMPDISP_SCREEN, readTemp);
}

void tubCommandSend(const AtCommand &command)
{
    commandResponse.clear();
    commandResponse.error = RERR_NONE;
    commandResponse.state = RS_WAITING;

    switch (command.type)
    {
    case AT_STAT:
        seqFullStat();
        break;
    case AT_ECHO:
        commandResponse.curTemp = command.param;
        commandResponse.setTemp = command.param;
        seqDone();
        break;
    case AT_RAW_DISP:
        seqButton(ButtonCode::STATUS_REQUEST, seqDone);
        break;
    case AT_RAW_MODE:
        seqButton(ButtonCode::BUTTON_MODE, seqDone);
        break;
    case AT_RAW_PLUS:
        seqPlus();
        break;
    case AT_RAW_MINUS:
        seqMinus();
        break;
    case AT_JETS1:
        seqScreenPlusMinus("JET1", command.param);
        break;
    case AT_JETS2:
        seqScreenPlusMinus("JET2", command.param);
        break;
    case AT_AIR:
        // My hot tub actually says "8LOWR" with an eight!
        seqScreenPlusMinus("?LOWR", command.param);
        break;
    case AT_CLEAN:
        seqScreenPlusMinus("CLEAN", command.param);
        break;
    case AT_SUMMER:
        seqScreenPlusMinus("STMR", command.param);
        break;
    case AT_TEMPSET:
        seqSetTemperature(command.param);
        break;
    case AT_TEMPADJ:
        // FIXME: Make sure +/- actually adjusts temperature, not just puts it in set temperature mode
        seqScreenPlusMinus(TEMPADJ_SCREEN, command.param);
        break;
    case AT_LIGHTSET:
        seqSetLight(command.param);
        break;
    case AT_LIGHTADJ:
        seqScreenPlusMinus("LITE", command.param);
        break;
    case AT_LOCKSPA:
        seqScreenPlusMinus("SPA", command.param);
        break;
    case AT_LOCKTEMP:
        seqScreenPlusMinus("LOCK", command.param);
        break;
    case AT_ERROR:
        commandResponse.error = RERR_BAD_COMMAND;
        seqDone();
        break;
    default:
        commandResponse.error = RERR_UNKNOWN_COMMAND;
        seqDone();
        break;
    }
}

void tubCommandAbort()
{
    currentHandler = nullptr;
    commandResponse.state = RS_VALID;
    commandResponse.error = RERR_ABORTED;
}

CommandResponse &tubCommandResponse()
{
    static constexpr uint8_t maxSeqHandler = 200; // Temperature set commands can take a long time
    static uint8_t seqHandlerNum = 0;
    parseButtonResponse();
    if (buttonResponse.valid())
    {
        commandResponse.copyStatusFrom(buttonResponse);
        buttonResponse.state = RS_EMPTY;
        if (buttonResponse.error != RERR_NONE)
        {
            commandResponse.state = RS_VALID;
            commandResponse.error = buttonResponse.error;
            buttonResponse.error = RERR_NONE;
        }
        else if (currentHandler)
        {
            //Serial.println("next");
            // The current handler should either set a new handler
            // or set the commandResponse state to valid if it's done.
            // Clear it here before calling to make sure we don't go
            // into a loop. This way, we'll error out below next time.
            handlerFn f = currentHandler;
            currentHandler = nullptr;
            if (++seqHandlerNum > maxSeqHandler) {
                commandResponse.state = RS_VALID;
                commandResponse.error = RERR_SEQ_TOO_LONG;
            } else {
                f();
            }
        }
        else if (commandResponse.state != RS_VALID)
        {
            commandResponse.state = RS_VALID;
            commandResponse.error = RERR_UNEXPECTED_SEQ_END;
        }
    }

    if (commandResponse.state == RS_VALID) {
        seqHandlerNum = 0;
    }

    return commandResponse;
}