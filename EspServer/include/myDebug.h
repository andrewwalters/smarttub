#ifndef DEBUG_PORT
#define DEBUG_PORT Serial1
#endif

#ifdef DEBUG_ENABLE
#define DPRINTF(...) DEBUG_PORT.printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif
