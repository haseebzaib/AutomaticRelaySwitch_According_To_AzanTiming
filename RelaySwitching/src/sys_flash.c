/**
 * @file sys_flash.c
 * @brief Flash memory management for the application.
 * 
 * This header file defines the interfaces and constants for interacting 
 * with the system's flash memory, including read, write, and erase operations.
 * 
 * @author Haseeb Zaib
 * @date December 19, 2024
 * @contact hzaib76@gmail.com
 */

#include "sys_flash.h"
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

/* Flash partition and device definitions */
#define TEST_PARTITION              storage_partition
#define TEST_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(TEST_PARTITION)
#define TEST_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(TEST_PARTITION)

/* Flash configuration constants */
#define FLASH_PAGE_SIZE             4096


const struct device *flash_dev;

/**
 * @brief Erases a portion of the flash memory.
 * 
 * This function erases a specific region of the flash memory, starting at the
 * given offset and covering the specified size. The operation is performed on
 * the partition defined by `TEST_PARTITION`.
 * 
 * @param offset Offset from the start of the partition to erase.
 * @param size Size of the memory region to erase (in bytes).
 * @return sys_flash_ok if successful, sys_flash_err if an error occurred.
 */
enum sys_flash_status sys_flash_erase(uint32_t offset, uint32_t size)
{
    enum sys_flash_status stat = sys_flash_ok;

    uint32_t flashOffset = TEST_PARTITION_OFFSET + offset;

    /* Attempt to erase the specified region */
    if (flash_erase(flash_dev, flashOffset, FLASH_PAGE_SIZE) != 0) {
        
        stat = sys_flash_err;
        goto common;
    }

common:
    return stat;
}

/**
 * @brief Reads data from the flash memory.
 * 
 * This function reads a specified amount of data from the flash memory,
 * starting at the given offset within the `TEST_PARTITION`.
 * 
 * @param offset Offset from the start of the partition to read from.
 * @param data Pointer to the buffer where the read data will be stored.
 * @param size Number of bytes to read.
 * @return sys_flash_ok if successful, sys_flash_err if an error occurred.
 */
enum sys_flash_status sys_flash_read(uint32_t offset, void *data, uint32_t size)
{
    enum sys_flash_status stat = sys_flash_ok;

    uint32_t flashOffset = TEST_PARTITION_OFFSET + offset;

    /* Attempt to read the specified data */
    if (flash_read(flash_dev, flashOffset, data, size) != 0) {
        
        stat = sys_flash_err;
        goto common;
    }

common:
    return stat;
}

/**
 * @brief Writes data to the flash memory.
 * 
 * This function writes a specified amount of data to the flash memory,
 * starting at the given offset within the `TEST_PARTITION`. Before writing,
 * it ensures the region is erased by calling `sys_flash_erase`.
 * 
 * @param offset Offset from the start of the partition to write to.
 * @param data Pointer to the data to be written.
 * @param size Number of bytes to write.
 * @return sys_flash_ok if successful, sys_flash_err if an error occurred.
 */
enum sys_flash_status sys_flash_write(uint32_t offset, void *data, uint32_t size)
{
    enum sys_flash_status stat = sys_flash_ok;

    uint32_t flashOffset = TEST_PARTITION_OFFSET + offset;

    /* Ensure the region is erased before writing */
    sys_flash_erase(offset, size);

    /* Attempt to write the specified data */
    if (flash_write(flash_dev, flashOffset, data, size) != 0) {
        
        stat = sys_flash_err;
        goto common;
    }

common:
    return stat;
}

/**
 * @brief Initializes the flash memory device.
 * 
 * This function initializes the flash memory device by retrieving the
 * device associated with the `TEST_PARTITION`. It verifies the device is
 * ready for use.
 * 
 * @return sys_flash_ok if successful, sys_flash_err if the device is not ready.
 */
enum sys_flash_status sys_flash_init()
{
    enum sys_flash_status stat = sys_flash_ok;
    flash_dev = TEST_PARTITION_DEVICE;



    /* Check if the flash device is ready */
    if (!device_is_ready(flash_dev)) {
        
        stat = sys_flash_err;
        goto common;
    }

common:
    return stat;
}


