// sd_pray2_io.c
#include "sd_pray2_io.h"
#include <zephyr/fs/fs.h>
#include <ctype.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
static const char *disk_mount_pt = "/SD:";

extern void print_uart(char *buf);

char logbuffer[250];
static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};


static int has_ext_bin_ci(const char *name) {
    size_t n = strlen(name);
    if (n < 4) return 0;
    const char *e = name + (n - 4);
    return (e[0]=='.') &&
           (tolower((unsigned char)e[1])=='b') &&
           (tolower((unsigned char)e[2])=='i') &&
           (tolower((unsigned char)e[3])=='n');
}

static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;


	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		//printk("Error opening dir %s [%d]\n", path, res);
            sprintf(logbuffer,"Error opening dir %s [%d]\r\n", path, res);
    print_uart(logbuffer);
		return res;
	}

	//printk("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			//printk("[DIR ] %s\n", entry.name);
                  sprintf(logbuffer,"[DIR ] %s\r\n", entry.name);
    print_uart(logbuffer);
		} else {
			// printk("[FILE] %s (size = %zu)\n",
			// 	entry.name, entry.size);

                 sprintf(logbuffer,"[FILE] %s (size = %zu)\r\n",entry.name, entry.size);
    print_uart(logbuffer);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

int mount_sd_card(void)
{
	/* raw disk i/o */
	static const char *disk_pdrv = "SD";
	uint64_t memory_size_mb;
	uint32_t block_count;
	uint32_t block_size;

	if (disk_access_init(disk_pdrv) != 0) {
		//LOG_ERR("Storage init ERROR!");
          sprintf(logbuffer,"Storage init ERROR!\r\n");
    print_uart(logbuffer);
		return -1;
	}

	if (disk_access_ioctl(disk_pdrv,
			DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
		//LOG_ERR("Unable to get sector count");
              sprintf(logbuffer,"Unable to get sector count\r\n");
    print_uart(logbuffer);
		return -1;
	}
	//LOG_INF("Block count %u", block_count);

          sprintf(logbuffer,"Unable to get sector count\r\n");
    print_uart(logbuffer);

	if (disk_access_ioctl(disk_pdrv,
			DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
		//LOG_ERR("Unable to get sector size");

        
          sprintf(logbuffer,"Unable to get sector size\r\n");
    print_uart(logbuffer);
		return -1;
	}
	//printk("Sector size %u\n", block_size);

           sprintf(logbuffer,"Sector size %u\r\n", block_size);
    print_uart(logbuffer);

	memory_size_mb = (uint64_t)block_count * block_size;
	//printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));

       sprintf(logbuffer,"Memory Size(MB) %u\r\n", (uint32_t)(memory_size_mb >> 20));
    print_uart(logbuffer);

	mp.mnt_point = disk_mount_pt;

	int res = fs_mount(&mp);

	if (res == FR_OK) {
		//printk("Disk mounted.\n");

        
       sprintf(logbuffer,"Disk mounted.\r\n");
    print_uart(logbuffer);
		lsdir(disk_mount_pt);
	} else {
		//printk("Failed to mount disk - trying one more time\n");
               sprintf(logbuffer,"Failed to mount disk - trying one more time\r\n");
    print_uart(logbuffer);
		res = fs_mount(&mp);
		if (res != FR_OK) {
			//printk("Error mounting disk.\n");
                sprintf(logbuffer,"Error mounting disk.\r\n");
    print_uart(logbuffer);
			return -1;
		}
	}

	return 0;
}

int sd_find_single_bin(const char *root, char *out_path, size_t out_len) {
    struct fs_dir_t dirp;
    struct fs_dirent ent;
    int rc, count = 0;
    char found[128];

    fs_dir_t_init(&dirp);
    rc = fs_opendir(&dirp, root);
    if (rc) return rc;

    for (;;) {
        rc = fs_readdir(&dirp, &ent);
        if (rc) break;
        if (ent.name[0] == 0) break;             // end of dir
        if (ent.type != FS_DIR_ENTRY_FILE) continue;
        if (!has_ext_bin_ci(ent.name)) continue; // only *.bin
        // skip hidden dotfiles just in case
        if (ent.name[0] == '.') continue;

        ++count;
        if (count == 1) {
            // Build "/SD:/filename.bin"
            if (snprintf(found, sizeof(found), "%s/%s", root, ent.name) >= (int)sizeof(found)) {
                fs_closedir(&dirp);
                return -1;
            }
        }
    }
    fs_closedir(&dirp);

    if (rc) return rc;
    if (count == 0) return -1;     // no .bin file
    if (count > 1)  return -1;     // more than one .bin; ask user to keep only one

    if (snprintf(out_path, out_len, "%s", found) >= (int)out_len) return -1;
    return 0;
}

int sd_load_entire_file(const char *path, uint8_t *buf, size_t max_len, size_t *out_len) {
    struct fs_file_t f;
    fs_file_t_init(&f);
    int rc = fs_open(&f, path, FS_O_READ);
    if (rc) return rc;

    size_t total = 0;
    for (;;) {
        uint8_t tmp[512];
        int r = fs_read(&f, tmp, sizeof(tmp));
        if (r < 0) { fs_close(&f); return r; }
        if (r == 0) break; // EOF
        if (total + (size_t)r > max_len) { fs_close(&f); return -1; }
        memcpy(buf + total, tmp, (size_t)r);
        total += (size_t)r;
    }
    fs_close(&f);
    if (out_len) *out_len = total;
    return 0;
}

int sd_clear_oneshot_flag_in_file(const char *path) {
    struct fs_file_t f;
    fs_file_t_init(&f);
    int rc = fs_open(&f, path, FS_O_RDWR);
    if (rc) return rc;

    // Seek to header flags byte (offset 14)
    rc = fs_seek(&f, 14, FS_SEEK_SET);
    if (rc) { fs_close(&f); return rc; }

    uint8_t flags = 0;
    int r = fs_read(&f, &flags, 1);
    if (r < 0) { fs_close(&f); return r; }

    uint8_t new_flags = (uint8_t)(flags & ~(PRAY2_FLAG_RTC_ONE_SHOT));
    if (new_flags != flags) {
        rc = fs_seek(&f, -1, FS_SEEK_CUR);
        if (rc) { fs_close(&f); return rc; }
        r = fs_write(&f, &new_flags, 1);
        if (r < 0) { fs_close(&f); return r; }
        (void)fs_sync(&f);
    }
    fs_close(&f);
    return 0;
}



static int write_file_all(struct fs_file_t *f, const uint8_t *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        size_t chunk = len - off;
        if (chunk > 1024) chunk = 1024;
        int w = fs_write(f, data + off, chunk);
        if (w < 0) return w;
        if (w == 0) return -EIO;
        off += (size_t)w;
    }
    return 0;
}

static int path_dir_and_name(const char *root, const char *name, char *out, size_t out_len) {
    int n = snprintf(out, out_len, "%s/%s", root, name);
    return (n < 0 || n >= (int)out_len) ? -ENAMETOOLONG : 0;
}

int sd_store_pray2_from_ram(const char *root,
                            const uint8_t *data, size_t len,
                            char *out_path, size_t out_len)
{
    char final_path[128];
    char temp_path[140];
    int rc;

    // Decide final filename
    rc = sd_find_single_bin(root, final_path, sizeof(final_path));
    if (rc == -ENOENT) {
        // No .bin present -> use default name
        rc = path_dir_and_name(root, "schedule.bin", final_path, sizeof(final_path));
        if (rc) return rc;
    } else if (rc == -EEXIST) {
        // More than one .bin present -> refuse to avoid ambiguity
        return rc;
    } else if (rc != 0) {
        return rc; // other error
    }

    // Build temp path "<final>.tmp"
    int n = snprintf(temp_path, sizeof(temp_path), "%s.tmp", final_path);
    if (n < 0 || n >= (int)sizeof(temp_path)) return -ENAMETOOLONG;

    // Ensure no stale temp file
    (void)fs_unlink(temp_path);

    // Create + write temp file
    struct fs_file_t f;
    fs_file_t_init(&f);
    rc = fs_open(&f, temp_path, FS_O_CREATE | FS_O_TRUNC | FS_O_WRITE);
    if (rc) return rc;

    rc = write_file_all(&f, data, len);
    if (rc == 0) (void)fs_sync(&f);
    (void)fs_close(&f);
    if (rc) { (void)fs_unlink(temp_path); return rc; }

    // If final exists, unlink before rename on some FAT stacks
    (void)fs_unlink(final_path);

    // Atomically move temp -> final
    rc = fs_rename(temp_path, final_path);
    if (rc) { (void)fs_unlink(temp_path); return rc; }

    if (out_path && out_len > 0) {
        int n2 = snprintf(out_path, out_len, "%s", final_path);
        if (n2 < 0 || n2 >= (int)out_len) return -ENAMETOOLONG;
    }
    return 0;
}
