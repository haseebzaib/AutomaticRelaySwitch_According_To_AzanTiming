/**
 * @file RTCmcp7940.h
 * @brief Header file for the MCP7940N RTC driver.
 *
 * This file defines the data structures, register mappings, and function prototypes
 * for interacting with the MCP7940N real-time clock (RTC) module. The driver includes
 * support for reading and setting date/time and accessing RTC control registers.
 *
 * @author Haseeb Zaib
 * @contact hzaib76@gmail.com
 * @date 2024-12-19
 */

#ifndef CUSTOM_MODULE_RTCMCP7940_H
#define CUSTOM_MODULE_RTCMCP7940_H

#include <zephyr/sys/timeutil.h>
#include <time.h>

/**
 * @brief MCP7940N RTC Seconds Register structure
 *
 * Represents the seconds register with individual bit fields.
 */
struct mcp7940n_rtc_sec {
	uint8_t sec_one : 4;  /**< Ones place of the seconds (0-9) */
	uint8_t sec_ten : 3;  /**< Tens place of the seconds (0-5) */
	uint8_t start_osc : 1; /**< Oscillator start/stop bit */
} __packed;

/**
 * @brief MCP7940N RTC Minutes Register structure
 *
 * Represents the minutes register with individual bit fields.
 */
struct mcp7940n_rtc_min {
	uint8_t min_one : 4; /**< Ones place of the minutes (0-9) */
	uint8_t min_ten : 3; /**< Tens place of the minutes (0-5) */
	uint8_t nimp : 1;    /**< Reserved/unused bit */
} __packed;

/**
 * @brief MCP7940N RTC Hours Register structure
 *
 * Represents the hours register with individual bit fields.
 */
struct mcp7940n_rtc_hours {
	uint8_t hr_one : 4;    /**< Ones place of the hours (0-9) */
	uint8_t hr_ten : 2;    /**< Tens place of the hours (0-2 for 24-hour mode) */
	uint8_t twelve_hr : 1; /**< 12-hour format indicator (0 = 24-hour, 1 = 12-hour) */
	uint8_t nimp : 1;      /**< Reserved/unused bit */
} __packed;

/**
 * @brief MCP7940N RTC Weekday Register structure
 *
 * Represents the weekday register with individual bit fields.
 */
struct mcp7940n_rtc_weekday {
	uint8_t weekday : 3; /**< Day of the week (0-6) */
	uint8_t vbaten : 1;  /**< Battery enable bit */
	uint8_t pwrfail : 1; /**< Power failure bit */
	uint8_t oscrun : 1;  /**< Oscillator running bit */
	uint8_t nimp : 2;    /**< Reserved/unused bits */
} __packed;

/**
 * @brief MCP7940N RTC Date Register structure
 *
 * Represents the date register with individual bit fields.
 */
struct mcp7940n_rtc_date {
	uint8_t date_one : 4; /**< Ones place of the date (0-9) */
	uint8_t date_ten : 2; /**< Tens place of the date (0-3) */
	uint8_t nimp : 2;     /**< Reserved/unused bits */
} __packed;

/**
 * @brief MCP7940N RTC Month Register structure
 *
 * Represents the month register with individual bit fields.
 */
struct mcp7940n_rtc_month {
	uint8_t month_one : 4; /**< Ones place of the month (0-9) */
	uint8_t month_ten : 1; /**< Tens place of the month (0-1) */
	uint8_t lpyr : 1;      /**< Leap year indicator */
	uint8_t nimp : 2;      /**< Reserved/unused bits */
} __packed;

/**
 * @brief MCP7940N RTC Year Register structure
 *
 * Represents the year register with individual bit fields.
 */
struct mcp7940n_rtc_year {
	uint8_t year_one : 4; /**< Ones place of the year (0-9) */
	uint8_t year_ten : 4; /**< Tens place of the year (0-9) */
} __packed;

/**
 * @brief MCP7940N RTC Control Register structure
 *
 * Represents the control register with individual bit fields.
 */
struct mcp7940n_rtc_control {
	uint8_t sqwfs : 2;    /**< Square wave frequency select */
	uint8_t crs_trim : 1; /**< Coarse trim enable */
	uint8_t ext_osc : 1;  /**< External oscillator enable */
	uint8_t alm0_en : 1;  /**< Alarm 0 enable */
	uint8_t alm1_en : 1;  /**< Alarm 1 enable */
	uint8_t sqw_en : 1;   /**< Square wave output enable */
	uint8_t out : 1;      /**< Output pin control */
} __packed;

/**
 * @brief MCP7940N Oscillator Trim Register structure
 *
 * Represents the oscillator trim register with individual bit fields.
 */
struct mcp7940n_rtc_osctrim {
	uint8_t trim_val : 7; /**< Oscillator trim value */
	uint8_t sign : 1;     /**< Trim value sign (0 = positive, 1 = negative) */
} __packed;

/**
 * @brief MCP7940N Time Register structure
 *
 * Aggregates all time-related registers for the MCP7940N RTC.
 */
struct mcp7940n_time_registers {
	struct mcp7940n_rtc_sec rtc_sec;       /**< Seconds register */
	struct mcp7940n_rtc_min rtc_min;       /**< Minutes register */
	struct mcp7940n_rtc_hours rtc_hours;   /**< Hours register */
	struct mcp7940n_rtc_weekday rtc_weekday; /**< Weekday register */
	struct mcp7940n_rtc_date rtc_date;     /**< Date register */
	struct mcp7940n_rtc_month rtc_month;   /**< Month register */
	struct mcp7940n_rtc_year rtc_year;     /**< Year register */
	struct mcp7940n_rtc_control rtc_control; /**< Control register */
	struct mcp7940n_rtc_osctrim rtc_osctrim; /**< Oscillator trim register */
} __packed;

/**
 * @brief Enum for MCP7940N register addresses
 *
 * Lists the register addresses for the MCP7940N RTC.
 */
enum mcp7940n_register {
	REG_RTC_SEC = 0x0,     /**< Seconds register */
	REG_RTC_MIN = 0x1,     /**< Minutes register */
	REG_RTC_HOUR = 0x2,    /**< Hours register */
	REG_RTC_WDAY = 0x3,    /**< Weekday register */
	REG_RTC_DATE = 0x4,    /**< Date register */
	REG_RTC_MONTH = 0x5,   /**< Month register */
	REG_RTC_YEAR = 0x6,    /**< Year register */
	REG_RTC_CONTROL = 0x7, /**< Control register */
	REG_RTC_OSCTRIM = 0x8, /**< Oscillator trim register */
	SRAM_MIN = 0x20,       /**< Start of SRAM */
	SRAM_MAX = 0x5F,       /**< End of SRAM */
	REG_INVAL = 0x60,      /**< Invalid register */
};

/**
 * @brief Structure for holding date/time information
 *
 * Represents a simplified version of date/time for the MCP7940N RTC.
 */
struct RTCmcp7940_TimeDate {
	uint8_t day;   /**< Day of the month (1-31) */
	uint8_t month; /**< Month of the year (1-12) */
	uint8_t year;  /**< Year (0-99, offset from 2000) */
	uint8_t hour;  /**< Hour of the day (0-23) */
	uint8_t min;   /**< Minute of the hour (0-59) */
	uint8_t sec;   /**< Second of the minute (0-59) */
};

/**
 * @brief Sets the date and time on the MCP7940N RTC.
 *
 * @param dev Pointer to the device structure.
 * @param time_str String representing the date/time to set.
 * @return 0 on success, or a negative error code on failure.
 */
int RTCmcp7940_set_datetime(const struct device *dev, char *time_str);

/**
 * @brief Gets the current date and time from the MCP7940N RTC.
 *
 * @param dev Pointer to the device structure.
 * @param time_str Buffer to store the retrieved date/time string.
 * @return 0 on success, or a negative error code on failure.
 */
int RTCmcp7940_get_datetime(const struct device *dev, char *time_str);

#endif