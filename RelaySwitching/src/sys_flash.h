/**
 * @file sys_flash.h
 * @brief Flash memory management for the application.
 * 
 * This header file defines the interfaces and constants for interacting 
 * with the system's flash memory, including read, write, and erase operations.
 * 
 * @author Haseeb Zaib
 * @date December 19, 2024
 * @contact hzaib76@gmail.com
 */

#ifndef _SYS_FLASH_H_
#define _SYS_FLASH_H_

#include <zephyr/kernel.h>


/**
 * @brief Enum for flash operation status.
 */
enum sys_flash_status {
  sys_flash_ok = 0, /**< Operation successful */
  sys_flash_err     /**< Operation failed */
};





/* Function declarations */
extern enum sys_flash_status sys_flash_erase(uint32_t offset, uint32_t size);
extern enum sys_flash_status sys_flash_read(uint32_t offset, void *data, uint32_t size);
extern enum sys_flash_status sys_flash_write(uint32_t offset, void *data, uint32_t size);
extern enum sys_flash_status sys_flash_init();



#endif /* _SYS_FLASH_H_ */