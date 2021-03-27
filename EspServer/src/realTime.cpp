#include "realTime.h"
#include "avrUpdater.h"

#include <time.h>
#include <sys/time.h>
#include <coredecls.h>

#include <ESP8266WiFi.h>

static void setTime_cb()
{
}

void setup_realTime()
{
    settimeofday_cb(setTime_cb);
    //configTime("CST+6CDT,M3.2.0/2,M11.1.0/2", "pool.ntp.org");
    configTime("PST+8PDT,M3.2.0/2,M11.1.0/2", "pool.ntp.org");
}

time_t getTime()
{
    //timeval tv;
    //struct timezone tz;
    //timespec tp;
    time_t tnow;

    //gettimeofday(&tv, &tz);
    //clock_gettime(0, &tp);
    tnow = time(nullptr);
    return tnow;
}

bool getTimeStruct(struct tm *buf)
{
    if (!buf) {
        return false;
    }
    time_t tnow = getTime();
    struct tm *res = localtime_r(&tnow, buf);
    if (res && buf->tm_year >= 100) {
        // Valid and year is 2000 or later
        return true;
    } else {
        return false;
    }
}