// sd_pray2_io.h
#pragma once
#include <zephyr/fs/fs.h>
#include <ff.h>
#include <stddef.h>
#include <stdint.h>

#ifndef PRAY2_FLAG_RTC_ONE_SHOT
#define PRAY2_FLAG_RTC_ONE_SHOT 0x10  // header flags bit4
#endif


int mount_sd_card(void);

// Find the single .bin file in root (e.g. "/SD:") and return full path "/SD:/xxx.bin".
// Returns 0 on success; negative errno or Zephyr FS error otherwise.
int sd_find_single_bin(const char *root, char *out_path, size_t out_len);

// Read entire file into RAM buffer. Sets *out_len.
// Returns 0 on success; negative errno/FS error otherwise.
int sd_load_entire_file(const char *path, uint8_t *buf, size_t max_len, size_t *out_len);

// Clear one-shot flag (bit 0x10) at header offset 14 in the file on SD.
// Safe to call even if bit already clear.
// Returns 0 on success; negative errno/FS error otherwise.
int sd_clear_oneshot_flag_in_file(const char *path);


int sd_store_pray2_from_ram(const char *root,
                            const uint8_t *data, size_t len,
                            char *out_path, size_t out_len);