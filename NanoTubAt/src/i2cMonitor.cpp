#include <Arduino.h>

static constexpr uint8_t monBufLogSize = 6;
static constexpr size_t monBufSize = 1 << monBufLogSize;
static constexpr uint8_t monBufPtrMask = monBufSize - 1;
static uint16_t monBuf[monBufSize];
static uint8_t monRdIdx = 0;
static uint8_t monWrIdx = 0;

// PCINT12 == A4 == SDA == PC[4]
// PCINT13 == A5 == SCL == PC[5]

void i2cMonitorSetup()
{
    cli();
    // Enable interrupts on PCINT12/13 (SDA/SCL)
    PCMSK1 |= 0x30;
    // Enable PC1 interrupts
    PCICR |= 0x02;
    // Clear any pending interrupt
    PCIFR |= 0x02;
    sei();
}

enum i2cState_t : uint8_t
{
    I2C_IDLE = 2,
    I2C_ADDR = 1,
    I2C_DATA = 0
};

#define I2C_ADDR_BIT 8
#define I2C_READ_BIT 9
#define I2C_ACK_BIT 10

void i2cMonitorLoop()
{
}

void i2cMonitorPrint()
{
    // { Ack', Read, Addr }
    static const char flagChars[8] = {' ', 'W', '?', 'R', '_', 'w', '!', 'r'};
    Serial.print("I2C");

    while (monRdIdx != monWrIdx)
    {
        uint8_t val = *((uint8_t*)(&monBuf[monRdIdx]));
        uint8_t flags = *((uint8_t*)(&monBuf[monRdIdx]) + 1);
        if (flags & (1 << (I2C_ADDR_BIT-8))) {
            flags |= (val & 1) << (I2C_READ_BIT-8);
            val >>= 1;
        }
        monRdIdx = (monRdIdx + 1) & monBufPtrMask;
        Serial.print(' ');
        Serial.print(flagChars[flags & 7]);
        Serial.print((val >> 4), 16);
        Serial.print(val & 0xf, 16);
    }
    Serial.println(' ');
}

#define SDA_BIT 4
#define SCL_BIT 5
#define SDA_MASK (1 << SDA_BIT)
#define SCL_MASK (1 << SCL_BIT)

struct i2cIsrStatus
{
    uint8_t i2cReg;
    uint8_t i2cCount;
    uint8_t port;
    i2cState_t i2cState;
};

ISR(PCINT1_vect)
{
    static struct i2cIsrStatus st = {
        0,
        0,
        SDA_MASK | SCL_MASK,
        I2C_IDLE};

    uint8_t port = PINC;
    while (true)
    {
        port &= (SDA_MASK | SCL_MASK);
        PCIFR |= _BV(PCIF1);
        if (st.i2cState == I2C_IDLE) {
            if (st.port == (SDA_MASK | SCL_MASK) && !(port & SDA_MASK)) {
                st.i2cState = I2C_ADDR;
                st.i2cCount = 0;
                st.port = port;
                return;
            }
        }
        else if (port & SCL_MASK)
        {
            if (st.port & SCL_MASK)
            {
                if (st.port & SDA_MASK)
                {
                    if (!(port & SDA_MASK))
                    {
                        st.i2cState = I2C_ADDR;
                        st.i2cCount = 0;
                        st.port = port;
                        return;
                    }
                }
                else
                {
                    if (port & SDA_MASK)
                    {
                        st.i2cState = I2C_IDLE;
                        st.i2cCount = 0;
                        st.port = port;
                        return;
                    }
                }
            }
            else
            {
                if (st.i2cCount < 8)
                {
                    st.i2cReg <<= 1;
                    st.i2cReg += (port & SDA_MASK) ? 1 : 0;
                    st.i2cCount++;
                }
                else
                {
                    uint8_t newIdx = (monWrIdx + 1) & monBufPtrMask;
                    if (newIdx != monRdIdx)
                    {
                        // 0 == DATA, 1 == ADDR
                        // Important not to change enum values or position of address flag!
                        uint8_t flags = (uint8_t)st.i2cState;
                        if (port & SDA_MASK) {
                            flags |= (1 << (I2C_ACK_BIT-8));
                        }
                        *((uint8_t*)(&monBuf[monWrIdx])) = st.i2cReg;
                        *((uint8_t*)(&monBuf[monWrIdx])+1) = flags;
                        // uint16_t val = (uint16_t)st.i2cReg;
                        // val |= (st.i2cState = I2C_ADDR) ? (1 << I2C_ADDR_BIT) : 0;
                        // val |= (uint16_t)(port & SDA_MASK) << (I2C_ACK_BIT - SDA_BIT);
                        // monBuf[monWrIdx] = val;
                        monWrIdx = newIdx;
                    }
                    st.i2cState = I2C_DATA;
                    st.i2cCount = 0;
                    st.port = port;
                    return;
                }
            }
        }
        st.port = port;
        uint8_t nochange = 10;
        while (((port ^ PINC) & 0x30) == 0)
        {
            if (--nochange == 0)
            {
                return;
            }
        }
        port = PINC;
    }
}
