#include "gps_parse.h"
#include "utils.h"
#include "debug.h"

int is_leap_year(int year) {
    year += 1900;
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int days_in_month(int month, int year) {
    static const int days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 1 && is_leap_year(year)) return 29;
    return days[month];
}

time_t mk_time(const struct minmea_date *date, const struct minmea_time *time_)
{
    int year = date->year;
    int month = date->month-1;
    int day = date->day-1;
    if (year < 80) year += 100;

    // Normalize month/year
    while (month < 0) { month += 12; year--; }
    while (month >= 12) { month -= 12; year++; }

    // Calculate days since 1970
    long days = 0;

    // Years
    for (int y = 70; y < year; y++)
        days += is_leap_year(y) ? 366 : 365;
    for (int y = year; y < 70; y--)
        days -= is_leap_year(y - 1) ? 366 : 365;

    // Months
    for (int m = 0; m < month; m++)
        days += days_in_month(m, year);

    // Days
    days += day;

    // Total seconds
    time_t total = days * 86400L +
                   time_->hours * 3600L +
                   time_->minutes * 60L +
                   time_->seconds;
 
    return total;
}
