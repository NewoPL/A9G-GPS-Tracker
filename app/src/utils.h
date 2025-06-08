#ifndef UTILS_H
#define UTILS_H

/**
 * @brief Test function for file system information.
 *
 * This function retrieves and prints file system information such as total size,
 * used space, and free space.
 */
void  FsInfoTest();

/**
 * @brief Trim leading and trailing whitespace from a string in place.
 *
 * @param str The string to trim.
 * @return Pointer to the trimmed string.
 */
char* trim_whitespace(char* str);

/**
 * @brief compare two strings in a case-insensitive manner.
 * 
 * This function compares two strings character by character, ignoring case differences.
 * 
 * @param s1 The first string to compare.
 * @param s2 The second string to compare.
 * @return An integer less than, equal to, or greater than zero if s1 is found,
 *         respectively, to be less than, to match, or be greater than s2.
 */
int str_case_cmp(const char *s1, const char *s2);

/**
 * @brief Convert a minmea date and time structure to a time_t value.
 * 
 * This function combines the date and time information from the provided
 * minmea_date and minmea_time structures to create a time_t value.
 * 
 * @param date Pointer to a minmea_date structure containing the date information.
 * @param time_ Pointer to a minmea_time structure containing the time information. 
 * @return time_t The combined date and time as a time_t value.
 * 
 */
time_t mk_time(const struct minmea_date *date, const struct minmea_time *time_);

/**
 * @brief Convert a CSQ (signal quality) value to a percentage.
 * 
 * This function takes a CSQ value (0-31) and converts it to a percentage
 * value (0-100). If the CSQ value is 99, it is treated as an unknown signal,
 * resulting in a percentage of 0.  
 *
 * @param csq The CSQ value to convert.
 * @return uint8_t The percentage representation of the CSQ value.
 * @note CSQ values outside the range of 0-31 are clamped to 31.
 * @note A CSQ value of 99 indicates an unknown or undetectable signal, resulting in a percentage of 0.
 */
uint8_t csq_to_percent(int csq);

#endif