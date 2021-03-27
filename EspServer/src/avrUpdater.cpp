#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <Schedule.h>

#include <stdarg.h>
#include <string.h>

#include "avrUpdater.h"

static constexpr int updatePort = 9898;
static constexpr unsigned long bootloaderRate = 19200;
static unsigned long appRate = 115200;

static AsyncClient *updateClient = nullptr;

static uint16_t packetOffset = 0;
static struct pbuf *packetList = nullptr;
static constexpr int circBufferBits = 7;
static constexpr int circBufferSize = 1 << circBufferBits;
static constexpr int circBufferMask = circBufferSize - 1;
static uint8_t circBuffer[circBufferSize];
static volatile int circRd = 0;
static volatile int circWr = 0;

static char debugPrintBuffer[512];
static int debugPrintOffset = 0;
static int debugPrintFilled = 0;
static bool preferSerial = false;

static bool newConnection = true;
static bool doingOta = false;

static void avrReset()
{
    pinMode(0, OUTPUT);
    digitalWrite(0, 0);
    delay(100);
    digitalWrite(0, 1);
    delay(100);
}

static void avrOtaSetup()
{
    doingOta = true;
    appRate = Serial.baudRate();
    Serial.updateBaudRate(bootloaderRate);
    avrReset();
}

static void avrOtaEnd()
{
    if (doingOta)
    {
        delay(500);
        avrReset();
        Serial.updateBaudRate(appRate);
        doingOta = false;
    }
}

static void fillCircBuffer()
{
    while (packetList)
    {
        int nextWr = (circWr + 1) & circBufferMask;
        if (nextWr != circRd)
        {
            circBuffer[circWr] = ((uint8_t *)packetList->payload)[packetOffset];
            circWr = nextWr;
            packetOffset++;
        }
        else if (packetList->len != 0)
        {
            break;
        }

        if (packetOffset >= packetList->len)
        {
            struct pbuf *b = packetList;
            packetList = packetList->next;
            packetOffset = 0;
            b->next = nullptr;
            updateClient->ackPacket(b);
        }
    }
}

static void handlePacket(void *, AsyncClient *, struct pbuf *pb)
{
    struct pbuf *tail = packetList;
    if (tail == nullptr)
    {
        tail = pb;
    }
    else
    {
        struct pbuf *b = tail;
        while (b->next)
        {
            b = b->next;
        }
        b->next = pb;
    }
    packetList = tail;
    fillCircBuffer();
}

static void handleDisconnect(void *arg, AsyncClient *client)
{
    updateClient = nullptr;
    packetList = nullptr;
    packetOffset = 0;
    schedule_function(avrOtaEnd);
}

static void handleNewClient(void *arg, AsyncClient *client)
{
    if (updateClient)
    {
        // Already have a connection
        client->close(true);
        client->free();
    }
    else
    {
        newConnection = true;
        updateClient = client;
        client->setNoDelay(true);
        client->onPacket(handlePacket, nullptr);
        client->onError([](void *arg, AsyncClient *client, int8_t error) {}, nullptr);
        client->onDisconnect(handleDisconnect, nullptr);
        client->onTimeout([](void *arg, AsyncClient *client, uint32_t time) {}, nullptr);
    }
}

void avrUpdater_start()
{
    AsyncServer *server = new AsyncServer(updatePort);
    server->onClient(&handleNewClient, server);
    server->begin();
}

// Called from loop()
void avrUpdater_loop()
{
    if (!updateClient || !updateClient->connected())
    {
        return;
    }

    if (!preferSerial && !doingOta && debugPrintFilled > 0 && updateClient->canSend() && updateClient->space() > 0)
    {
        // Send any buffered debug output
        size_t toSend = std::min(updateClient->space(), static_cast<size_t>(debugPrintFilled-debugPrintOffset));
        updateClient->add(&debugPrintBuffer[debugPrintOffset], toSend);
        updateClient->send();
        debugPrintOffset += toSend;
        if (debugPrintOffset >= debugPrintFilled) {
            // Finished sending, reset buffer
            debugPrintOffset = 0;
            debugPrintFilled = 0;
        }
    }
    else if (Serial.available() > 0 && updateClient->canSend() && updateClient->space() > 0 && !newConnection)
    {
        // Read serial data and send to TCP connection
        static constexpr size_t maxSend = 128;
        size_t toSend = std::min({static_cast<size_t>(Serial.available()), updateClient->space(), maxSend});
        char buffer[maxSend];
        toSend = Serial.readBytes(buffer, toSend);
        updateClient->add(buffer, toSend);
        updateClient->send();
        preferSerial = true;
    }
    else if (newConnection)
    {
        // Discard incoming data on new connection until we send our first byte
        while (Serial.available())
        {
            Serial.read();
        }
    }

    if (Serial.available() == 0) {
        preferSerial = false;
    }

    // Grab incoming TCP data if needed
    fillCircBuffer();

    // Send incoming TCP data over serial port
    if (circWr != circRd)
    {
        if (newConnection)
        {
            // <Enter> is an escape for doing terminal debug instead of OTA
            uint8_t ch = circBuffer[circRd];
            if (ch != '\r' && ch != '\n')
            {
                avrOtaSetup();
            }
            newConnection = false;
        }
        int wr = circWr;
        int toSend = ((circRd < wr) ? wr : circBufferSize) - circRd;
        toSend = std::min(toSend, Serial.availableForWrite());
        if (toSend > 0)
        {
            toSend = Serial.write(&circBuffer[circRd], static_cast<size_t>(toSend));
            circRd = (circRd + toSend) & circBufferMask;
        }
    }
}

void debugPrintf(const char* format, ...)
{
    if (!updateClient || !updateClient->connected())
    {
        return;
    }

    va_list args, args1;
    va_start(args, format);
    va_copy(args1, args);
    int toWrite = vsnprintf(nullptr, 0, format, args);
    va_end(args1);
    if (toWrite > 0) {
        if (debugPrintOffset > 0) {
            // Compact buffer first
            debugPrintFilled -= debugPrintOffset;
            memmove(&debugPrintBuffer[0], &debugPrintBuffer[debugPrintOffset], debugPrintFilled);
            debugPrintOffset = 0;
        }
        int freeSpace = static_cast<int>(sizeof(debugPrintBuffer)) - debugPrintFilled;
        if (toWrite <= freeSpace) {
            int written = vsnprintf(&debugPrintBuffer[debugPrintFilled], freeSpace, format, args);
            if (written > 0 && written <= freeSpace) {
                debugPrintFilled += written;
            }
        }
    }
    va_end(args);
}