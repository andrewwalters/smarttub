#ifndef _COMMANDS_HPP_
#define _COMMANDS_HPP_

#include <Arduino.h>

// AT command definitions, shared between projects
enum AtCommandType
{
    AT_STAT = 0,
    AT_ECHO,
    AT_RAW_DISP,
    AT_RAW_MODE,
    AT_RAW_PLUS,
    AT_RAW_MINUS,
    AT_JETS1,
    AT_JETS2,
    AT_AIR,
    AT_CLEAN,
    AT_SUMMER,
    AT_TEMPSET,
    AT_TEMPADJ,
    AT_LIGHTSET,
    AT_LIGHTADJ,
    AT_LOCKSPA,
    AT_LOCKTEMP,
    AT_I2CINFO,
    AT_RESET,
    AT_NUM_COMMANDS,
    AT_NONE = 128,
    AT_ERROR
};

extern const char* atCmdList[AT_NUM_COMMANDS];

struct AtCommand
{
    AtCommand() : type(AT_NONE), param(0) {}
    AtCommandType type;
    int param;
};

#endif // _COMMANDS_HPP_
