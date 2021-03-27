#include <Ticker.h>

#include "realTime.h"
#include "avrUpdater.h"

static Ticker timer;

enum dayOfWeek : uint8_t {
    DW_NONE = 0,
    DW_SUNDAY = 0x01,
    DW_MONDAY = 0x02,
    DW_TUESDAY = 0x04,
    DW_WEDNESDAY = 0x08,
    DW_THURSDAY = 0x10,
    DW_FRIDAY = 0x20,
    DW_SATURDAY = 0x40,
    DW_WEEKDAY = DW_MONDAY | DW_TUESDAY | DW_WEDNESDAY | DW_THURSDAY | DW_FRIDAY,
    DW_WEEKEND = DW_SUNDAY | DW_SATURDAY,
    DW_ALL = DW_WEEKDAY | DW_WEEKEND
};

static constexpr int timeToSeconds(int hour, int minute = 0, int second = 0)
{
    return (second + 60 * (minute + 60 * hour));
}

typedef struct {
    int seconds;
    dayOfWeek days;
    uint8_t temperature;
    bool fixed;
} progSetting;

static progSetting settings[] = {
    { timeToSeconds(0, 0, 0), DW_ALL, 98, true },
    { timeToSeconds(5, 0, 0), DW_ALL, 102, true },
    { timeToSeconds(9, 30, 0), DW_ALL, 100, true },
    { timeToSeconds(11, 30, 0), DW_WEEKDAY, 101, true },
    { timeToSeconds(15, 45, 0), DW_WEEKDAY, 104, true },
    { timeToSeconds(16, 0, 0), DW_WEEKDAY, 100, true },
    { timeToSeconds(16, 30, 0), DW_WEEKEND, 102, true },
    { timeToSeconds(17, 30, 0), DW_ALL, 102, true },
    { timeToSeconds(23, 0, 0), DW_ALL, 98, true }
};

static constexpr int numSettings = static_cast<int>(sizeof(settings)/sizeof(settings[0]));

static int currentSetting = -1;
static int newSetting = -1;

static int lookupSetting(const struct tm& now)
{
    int closestIdx = -1;
    int closestDelta = 24 * 3600;
    for (int i = 0; i < numSettings; i++) {
        const progSetting& setting = settings[i];
        if ((1 << now.tm_wday) & static_cast<uint8_t>(setting.days)) {
            int delta = timeToSeconds(now.tm_hour, now.tm_min, now.tm_sec) - setting.seconds;
            if (delta > 0 && delta < closestDelta) {
                closestDelta = delta;
                closestIdx = i;
            } 
        }
    }

    return closestIdx;
}

static void checkTimer()
{
    debugPrintf("checkTimer()\n");
    struct tm now;
    if (getTimeStruct(&now)) {
        newSetting = lookupSetting(now);
        debugPrintf("lookup: %02d:%02d:%02d %d -> %d\n", now.tm_hour, now.tm_min, now.tm_sec, currentSetting, newSetting);
    }
}

void progTimer_setup()
{
    currentSetting = -1;
    timer.attach_ms_scheduled(10000, checkTimer);
}

bool progTimer_getSetting(int* temperature)
{
    if (newSetting >= 0 && currentSetting != newSetting) {
        currentSetting = newSetting;
        *temperature = settings[currentSetting].temperature;
        return true;
    }

    return false;
}
