#include <stdbool.h>
#include <api_inc_time.h>

#include "config_store.h"
#include "minmea.h"
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
    if (year < 70) year += 100;

    // Normalize month/year
    while (month < 0) { month += 12; year--; }
    while (month >= 12) { month -= 12; year++; }

    // Calculate days since 1970
    long days = 0;

    // Years
    for (int y = 70; y < year; y++)
        days += is_leap_year(y) ? 366 : 365;
    
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

int str_case_cmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = tolower((unsigned char)*s1);
        char c2 = tolower((unsigned char)*s2);

        if (c1 != c2) {
            return (unsigned char)c1 - (unsigned char)c2;
        }

        s1++;
        s2++;
    }

    // If both strings end together, returns 0
    // If not, compare final characters
    return (unsigned char)tolower((unsigned char)*s1) - 
           (unsigned char)tolower((unsigned char)*s2);
}

char* trim_whitespace(char* str)
{
    if (!str) return NULL;

    // trim leading whitespace
    while (isspace((unsigned char)*str)) str++;

    // if there were only whitespaces
    if (*str == '\0') return str;

    // trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}
