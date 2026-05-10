#include "rtc.h"

static const int days_before_month[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

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
    uint8_t rawCentury = isBcd ? 0x20 : 20;
    
    uint8_t century_reg = GetCenturyReg();
    if (century_reg != 0) {
        rawCentury = ReadRtcRegister(century_reg);
    }

    if (isBcd) {
        out->day   = BcdToBin(rawDay);
        out->month = BcdToBin(rawMonth);
        out->year  =  (BcdToBin(rawCentury) * 100) + BcdToBin(rawYear);
    } else {
        out->day   = rawDay;
        out->month = rawMonth;
        out->year  = (rawCentury * 100) + rawYear;
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
        if (rawHour & 0x80) rawHour = (rawHour & 0x7F) + 12;
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

uint32_t CalculateUnixTimestamp(total_time_t* total_time) {
    if (total_time == NULL) return 0;

    uint64_t total_days = 0;

    for (int y = 1970; y < total_time->date.year; y++) {
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            total_days += 366; 
        } else {
            total_days += 365;
        }
    }

    total_days += days_before_month[total_time->date.month - 1];

    if (total_time->date.month > 2 && ((total_time->date.year % 4 == 0 && total_time->date.year % 100 != 0) || (total_time->date.year % 400 == 0))) {
        total_days++;
    }

    total_days += (total_time->date.day - 1);

    uint32_t timestamp = (total_days * 86400) + (total_time->time.hours * 3600) + (total_time->time.minutes * 60) + total_time->time.seconds;

    return timestamp;
}