
#include "commands.hpp"

// Command list must be in same order as enum,
// starting with FIRST_COMMAND

static const char sAT_STAT[] PROGMEM = "AT.STAT";
static const char sAT_ECHO[] PROGMEM = "AT.ECHO";
static const char sAT_DISP[] PROGMEM = "D";
static const char sAT_MODE[] PROGMEM = "M";
static const char sAT_PLUS[] PROGMEM = "+";
static const char sAT_MINUS[] PROGMEM = "-";
static const char sAT_JETS1[] PROGMEM = "AT.JETS1";
static const char sAT_JETS2[] PROGMEM = "AT.JETS2";
static const char sAT_AIR[] PROGMEM = "AT.AIR";
static const char sAT_CLEAN[] PROGMEM = "AT.CLEAN";
static const char sAT_SUMMER[] PROGMEM = "AT.SUMMER";
static const char sAT_TEMPSET[] PROGMEM = "AT.TEMPSET";
static const char sAT_TEMPADJ[] PROGMEM = "AT.TEMPADJ";
static const char sAT_LIGHTSET[] PROGMEM = "AT.LIGHTSET";
static const char sAT_LIGHTADJ[] PROGMEM = "AT.LIGHTADJ";
static const char sAT_LOCKSPA[] PROGMEM = "AT.LOCKSPA";
static const char sAT_LOCKTEMP[] PROGMEM = "AT.LOCKTEMP";
static const char sAT_I2CINFO[] PROGMEM = "I";
static const char sAT_RESET[] PROGMEM = "ATZ";

const char* atCmdList[AT_NUM_COMMANDS] = {
    sAT_STAT,
    sAT_ECHO,
    sAT_DISP,
    sAT_MODE,
    sAT_PLUS,
    sAT_MINUS,
    sAT_JETS1,
    sAT_JETS2,
    sAT_AIR,
    sAT_CLEAN,
    sAT_SUMMER,
    sAT_TEMPSET,
    sAT_TEMPADJ,
    sAT_LIGHTSET,
    sAT_LIGHTADJ,
    sAT_LOCKSPA,
    sAT_LOCKTEMP,
    sAT_I2CINFO,
    sAT_RESET
};
