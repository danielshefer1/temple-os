#include "rtc.h"

uint8_t ReadRtcRegister(uint8_t reg) {
    outb(RTC_OUT_PORT, reg);
    return inb(RTC_IN_PORT);
}

uint8_t BcdToBin(uint8_t bcd) {
    return ((bcd / 16) * 10) + (bcd % 16);
}

void WaitForRtcUpdate() {
    outb(RTC_OUT_PORT, RTC_HANG_REG);
    while (inb(RTC_IN_PORT) & 0x80); 
}

int64_t GetDate(date_t* out) {
    if (out == NULL) return 1;

    WaitForRtcUpdate();

    uint8_t statusB = ReadRtcRegister(RTC_STATUS_REG);
    bool isBcd = !(statusB & 0x04);

    uint8_t rawDay   = ReadRtcRegister(RTC_DAY_REG);
    uint8_t rawMonth = ReadRtcRegister(RTC_MONTH_REG);
    uint8_t rawYear  = ReadRtcRegister(RTC_YEAR_REG);

    if (isBcd) {
        out->day   = BcdToBin(rawDay);
        out->month = BcdToBin(rawMonth);
        out->year  = BcdToBin(rawYear);
    } else {
        out->day   = rawDay;
        out->month = rawMonth;
        out->year  = rawYear;
    }

    return 0;
}

int64_t GetTime(time_t* out) {
    if (out == NULL) return 1;

    WaitForRtcUpdate();

    uint8_t statusB = ReadRtcRegister(RTC_STATUS_REG);
    bool isBcd = !(statusB & 0x04);
    bool is24Hour = statusB & 0x02;

    uint8_t rawSec   = ReadRtcRegister(RTC_SEC_REG);
    uint8_t rawMin = ReadRtcRegister(RTC_MIN_REG);
    uint8_t rawHour  = ReadRtcRegister(RTC_HOUR_REG);

    if (!is24Hour) {
        rawHour = (rawHour & (1 << 7) == 1) ? rawHour += 12 : rawHour;
    }

    if (isBcd) {
        out->seconds   = BcdToBin(rawSec);
        out->minutes = BcdToBin(rawMin);
        out->hours  = BcdToBin(rawHour);
    } else {
        out->seconds   = rawSec;
        out->minutes = rawMin;
        out->hours  = rawHour;
    }

    return 0;
}

int64_t GetTotalTime(total_time_t* out) {
    if (out == NULL) return 1;

    int64_t ret_date = GetDate(&out->date), ret_time = GetTime(&out->time);
    return ret_date + ret_time;
}