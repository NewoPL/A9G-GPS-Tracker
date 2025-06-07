#ifndef UTILS_H
#define UTILS_H

time_t mk_time(const struct minmea_date *date, const struct minmea_time *time_);

/**
 * @brief Trim leading and trailing whitespace from a string in place.
 *
 * @param str The string to trim.
 * @return Pointer to the trimmed string.
 */
char* trim_whitespace(char* str);

int   str_case_cmp(const char *s1, const char *s2);

#endif