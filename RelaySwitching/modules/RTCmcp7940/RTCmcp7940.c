/**
 * @file RTCmcp7940.c
 * @brief Implementation file for the MCP7940N RTC driver.
 *
 * This file contains the implementation of the MCP7940N RTC driver, including
 * functions for reading and writing RTC registers, setting and getting the current
 * date/time, and initializing the device.
 *
 * @author Haseeb Zaib
 * @contact hzaib76@gmail.com
 * @date 2024-12-19
 */

#define DT_DRV_COMPAT zephyr_rtcmcp7940

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <RTCmcp7940.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/sys/util.h>
#include <time.h>

LOG_MODULE_REGISTER(RTCMCP7940, CONFIG_CUSTOM_RTCMCP7940_LOG_LEVEL);

/* Alarm channels */
#define ALARM0_ID			0  /**< Alarm 0 channel */
#define ALARM1_ID			1  /**< Alarm 1 channel */

/* Size of block when writing the entire struct */
#define RTC_TIME_REGISTERS_SIZE		sizeof(struct mcp7940n_time_registers)

/* Maximum I2C write block size */
#define MAX_WRITE_SIZE                  (RTC_TIME_REGISTERS_SIZE)

/**
 * @brief Configuration structure for MCP7940N
 *
 * Contains the I2C and GPIO configuration for the device.
 */
struct mcp7940n_config {
	struct i2c_dt_spec i2c;          /**< I2C specification */
	const struct gpio_dt_spec int_gpios; /**< Interrupt GPIO specification */
};

/**
 * @brief Runtime data structure for MCP7940N
 *
 * Stores runtime information such as register data and locks.
 */
struct mcp7940n_data {
	const struct device *mcp7940n;         /**< Pointer to the device */
	struct k_sem lock;                     /**< Semaphore for device access */
	struct mcp7940n_time_registers registers; /**< Cached register values */
	struct gpio_callback int_callback;     /**< Interrupt callback structure */
	bool int_active_high;                  /**< Interrupt polarity */
};

/* Macros for BCD conversion */
#define RTC_BCD_DECODE(reg_prefix) (reg_prefix##_one + reg_prefix##_ten * 10)
#define BCD_TO_BIN(val)       (((val) >> 4) * 10 + ((val) & 0x0F))
#define BIN_TO_BCD(val)       ((((val) / 10) << 4) | ((val) % 10))

/**
 * @brief Reads a single register from MCP7940N.
 *
 * @param dev Pointer to the device structure.
 * @param addr Address of the register to read.
 * @param val Pointer to store the read value.
 * @return 0 on success, or a negative error code on failure.
 */
static int read_register(const struct device *dev, uint8_t addr, uint8_t *val)
{
	const struct mcp7940n_config *cfg = dev->config;

	int rc = i2c_write_read_dt(&cfg->i2c, &addr, sizeof(addr), val, 1);

	return rc;
}

/**
 * @brief Writes a single register to MCP7940N.
 *
 * @param dev Pointer to the device structure.
 * @param addr Address of the register to write.
 * @param value Value to write to the register.
 * @return 0 on success, or a negative error code on failure.
 */
static int write_register(const struct device *dev, enum mcp7940n_register addr, uint8_t value)
{
	const struct mcp7940n_config *cfg = dev->config;
	uint8_t time_data[2] = {addr, value};

	return i2c_write_dt(&cfg->i2c, time_data, sizeof(time_data));
}

/**
 * @brief Writes a block of data to MCP7940N registers.
 *
 * @param dev Pointer to the device structure.
 * @param addr Starting address of the registers to write.
 * @param size Number of bytes to write.
 * @return 0 on success, or a negative error code on failure.
 */
static int write_data_block(const struct device *dev, enum mcp7940n_register addr, uint8_t size)
{
	struct mcp7940n_data *data = dev->data;
	const struct mcp7940n_config *cfg = dev->config;
	uint8_t time_data[MAX_WRITE_SIZE + 1];
	uint8_t *write_block_start;

	if (size > MAX_WRITE_SIZE) {
		return -EINVAL;
	}

	if (addr >= REG_INVAL) {
		return -EINVAL;
	}

	if (addr == REG_RTC_SEC) {
		write_block_start = (uint8_t *)&data->registers;
	} else {
		return -EINVAL;
	}

	time_data[0] = addr;
	memcpy(&time_data[1], write_block_start, size);

	return i2c_write_dt(&cfg->i2c, time_data, size + 1);
}

/**
 * @brief Starts the RTC counter by enabling the oscillator.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, or a negative error code on failure.
 */
static int mcp7940n_counter_start(const struct device *dev)
{
	struct mcp7940n_data *data = dev->data;

	data->registers.rtc_sec.start_osc = 1;
	return write_register(dev, REG_RTC_SEC,
		*((uint8_t *)(&data->registers.rtc_sec)));
}

/**
 * @brief Retrieves the current date and time from MCP7940N.
 *
 * @param dev Pointer to the device structure.
 * @param time_str Buffer to store the formatted date/time string.
 * @return 0 on success, or a negative error code on failure.
 */
int RTCmcp7940_get_datetime(const struct device *dev, char *time_str)
{
	struct mcp7940n_data *data = dev->data;
	const struct mcp7940n_config *cfg = dev->config;
	uint8_t addr = REG_RTC_SEC;

		//	k_sem_take(&data->lock, K_FOREVER);

	int rc = i2c_write_read_dt(&cfg->i2c, &addr, sizeof(addr), &data->registers,
				   RTC_TIME_REGISTERS_SIZE);

	if (rc < 0) {
		LOG_ERR("Failed to read datetime");
		return rc;
	}

	snprintf(time_str, 18, "%02d:%02d:%02d|%02d/%02d/%02d",
	         RTC_BCD_DECODE(data->registers.rtc_hours.hr),
	         RTC_BCD_DECODE(data->registers.rtc_min.min),
	         RTC_BCD_DECODE(data->registers.rtc_sec.sec),
	         RTC_BCD_DECODE(data->registers.rtc_date.date),
	         RTC_BCD_DECODE(data->registers.rtc_month.month),
	         RTC_BCD_DECODE(data->registers.rtc_year.year));

		//k_sem_give(&data->lock);
	return 0;
}

/**
 * @brief Sets the date and time on MCP7940N.
 *
 * @param dev Pointer to the device structure.
 * @param time_str Formatted date/time string (e.g., HH:MM:SS|DD/MM/YY).
 * @return 0 on success, or a negative error code on failure.
 */
int RTCmcp7940_set_datetime(const struct device *dev, char *time_str)
{
	struct mcp7940n_data *data = dev->data;

	//k_sem_take(&data->lock, K_FOREVER);

	if (strlen(time_str) != 17 || time_str[8] != '|' || time_str[11] != '/' || time_str[14] != '/') {
		LOG_ERR("Invalid time format. Expected HH:MM:SS|DD/MM/YY");
		return -EINVAL;
	}

	int hour, minutes, seconds, date, month, year;
	if (sscanf(time_str, "%02d:%02d:%02d|%02d/%02d/%02d", &hour, &minutes, &seconds, &date, &month, &year) != 6) {
		LOG_ERR("Failed to parse time string");
		return -EINVAL;
	}

	data->registers.rtc_sec.start_osc = 1;
	data->registers.rtc_sec.sec_one = seconds % 10;
	data->registers.rtc_sec.sec_ten = seconds / 10;
	data->registers.rtc_min.min_one = minutes % 10;
	data->registers.rtc_min.min_ten = minutes / 10;
	data->registers.rtc_hours.hr_one = hour % 10;
	data->registers.rtc_hours.hr_ten = hour / 10;
	data->registers.rtc_date.date_one = date % 10;
	data->registers.rtc_date.date_ten = date / 10;
	data->registers.rtc_month.month_one = month % 10;
	data->registers.rtc_month.month_ten = month / 10;
	data->registers.rtc_year.year_one = year % 10;
	data->registers.rtc_year.year_ten = year / 10;

    int retrn = write_data_block(dev, REG_RTC_SEC, RTC_TIME_REGISTERS_SIZE);

	//k_sem_give(&data->lock);
	
	return retrn;
}

/**
 * @brief Initializes the MCP7940N device.
 *
 * @param dev Pointer to the device structure.
 * @return 0 on success, or a negative error code on failure.
 */
static int mcp7940n_init(const struct device *dev)
{
	struct mcp7940n_data *data = dev->data;
	const struct mcp7940n_config *cfg = dev->config;

	//k_sem_init(&data->lock, 0, 1);

		// k_sem_take(&data->lock,K_FOREVER);

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("I2C device %s is not ready", cfg->i2c.bus->name);
		return -ENODEV;
	}

	char get_time[20];
	RTCmcp7940_get_datetime(dev, get_time);

	data->registers.rtc_hours.twelve_hr = false;
	int rc = write_register(dev, REG_RTC_HOUR, *((uint8_t *)(&data->registers.rtc_hours)));

	if (rc < 0) {
		return rc;
	}

        int rtn = mcp7940n_counter_start(dev);



		//k_sem_give(&data->lock);

			return rtn;
}

#define INST_DT_MCP7904N(index)                                                         \
	static struct mcp7940n_data mcp7940n_data_##index;				\
	static const struct mcp7940n_config mcp7940n_config_##index = {			\
		.i2c = I2C_DT_SPEC_INST_GET(index),					\
		.int_gpios = GPIO_DT_SPEC_INST_GET_OR(index, int_gpios, {0}),		\
	};										\
	DEVICE_DT_INST_DEFINE(index, mcp7940n_init, NULL,				\
		    &mcp7940n_data_##index,						\
		    &mcp7940n_config_##index,						\
		    POST_KERNEL,							\
		    CONFIG_CUSTOM_RTCMCP7940_INIT_PRIORITY,					\
		    NULL);

DT_INST_FOREACH_STATUS_OKAY(INST_DT_MCP7904N);