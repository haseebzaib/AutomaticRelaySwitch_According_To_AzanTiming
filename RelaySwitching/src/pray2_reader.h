// pray2_sched.h — PRAY2 v2 (no-CRC) parser + 1 Hz scheduler
// C99, little-endian safe, header-only. Public domain / MIT-style.

#ifndef PRAY2_SCHED_H
#define PRAY2_SCHED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>


extern void print_uart(char *buf);

extern const struct device *RTC_MCP;

// ====== PRAY2 v2 header spec (64 bytes) ======
//  0  char[5]  magic = "PRAY2"
//  5  u8       version = 2
//  6  u16      header_size = 64
//  8  u16      year (nominal; start date's year)
// 10  u16      days (count of days in table)
// 12  u8       start_month (1..12)
// 13  u8       start_day   (1..31)
// 14  u8       flags (bit0: per-day durations present; currently 0 in our generator)
// 15  u8       method_code (0..6; informational)
// 16  char[17] rtc_str_local = "HH:MM:SS|DD/MM/YY"  (or "...|DD:MM:YY"; no NUL)
// 33  u8       pad = 0
// 34  u16[5]   default_on_sec (Fajr..Isha) (seconds)
// 44  u32      table_offset
// 48  u32      table_size   (= days*5*2)
// 52  u32      durations_offset (0 if none)
// 56  u32      durations_size   (0 if none)
// 60  u16      reserved1 = 0
// 62  u16      reserved2 = 0
// Then: times table (days × 5 × u16 minutes).  (CRC may be present in file, but ignored here.)

#define PRAY2_HEADER_SIZE 64
#define PRAY2_MAGIC "PRAY2"
#define PRAY2_VERSION 2

#define PRAY2_FLAG_RTC_ONE_SHOT 0x10  // header flags bit4

typedef struct {
    // header fields
    uint16_t year;
    uint16_t days;
    uint8_t  start_month;
    uint8_t  start_day;
    uint8_t  flags;
    uint8_t  method_code;
    char     rtc_ascii[18];       // "HH:MM:SS|DD/MM/YY" or "HH:MM:SS|DD:MM:YY", NUL-terminated
    uint16_t default_on_sec[5];   // seconds
    uint32_t table_offset;
    uint32_t table_size;
    uint32_t durations_offset;
    uint32_t durations_size;
    // derived pointers into the supplied buffer
    const uint8_t* table_ptr;     // not owned
    const uint8_t* durations_ptr; // NULL if not present
} pray2_header_t;

// Little-endian readers (no unaligned casts).
static inline uint16_t pray2_rd_u16le(const uint8_t* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t pray2_rd_u32le(const uint8_t* p) {
    return (uint32_t)(p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}


// ---- small date helpers ----
static inline bool is_leap(int y) {
    return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0);
}
static inline int days_in_month(int y, int m) {
    static const uint8_t dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2) return dim[1] + (is_leap(y) ? 1 : 0);
    return dim[m-1];
}
static inline void advance_one_day(int *y, int *m, int *d) {
    int dim = days_in_month(*y, *m);
    (*d)++;
    if (*d > dim) { *d = 1; (*m)++; if (*m > 12) { *m = 1; (*y)++; } }
}

// Days-from-civil (Hinnant), for robust index math across leap years.
static int64_t pray2_days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d - 1;
    const unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;
    return era * 146097 + (int)doe - 719468; // days since 1970-01-01
}
static int pray2_days_between(int y1,int m1,int d1, int y2,int m2,int d2){
    int64_t a = pray2_days_from_civil(y1,(unsigned)m1,(unsigned)d1);
    int64_t b = pray2_days_from_civil(y2,(unsigned)m2,(unsigned)d2);
    int64_t diff = b - a;
    if (diff < -2147483648LL) diff = -2147483648LL;
    if (diff >  2147483647LL) diff =  2147483647LL;
    return (int)diff;
}

// Parse "HH:MM:SS|DD/MM/YY"  (also accepts "DD:MM:YY")
static bool pray2_parse_rtc_ascii(const char* s17,
                                  int* out_h, int* out_m, int* out_s,
                                  int* out_D, int* out_M, int* out_Y_full)
{
    if (!s17) return false;
    // expected positions:  01234567890123456
    //                      HH:MM:SS|DD/MM/YY
    if (!(s17[2]==':' && s17[5]==':' && s17[8]=='|')) return false;
    char sep1 = s17[11], sep2 = s17[14];
    if (!((sep1=='/'||sep1==':') && (sep2=='/'||sep2==':'))) return false;

    int HH = (s17[0]-'0')*10 + (s17[1]-'0');
    int MM = (s17[3]-'0')*10 + (s17[4]-'0');
    int SS = (s17[6]-'0')*10 + (s17[7]-'0');
    int DD = (s17[9]-'0')*10 + (s17[10]-'0');
    int MO = (s17[12]-'0')*10 + (s17[13]-'0');
    int YY = (s17[15]-'0')*10 + (s17[16]-'0');

    if (HH<0||HH>23||MM<0||MM>59||SS<0||SS>59) return false;
    if (MO<1||MO>12||DD<1||DD>31) return false;

    int Y_full = 2000 + YY;  // tweak if you want a different century rule
    if (out_h) *out_h = HH;
    if (out_m) *out_m = MM;
    if (out_s) *out_s = SS;
    if (out_D) *out_D = DD;
    if (out_M) *out_M = MO;
    if (out_Y_full) *out_Y_full = Y_full;
    return true;
}

// ===== Validation without CRC (XMODEM padding tolerated) =====
typedef enum {
    PRAY2_OK = 0,
    PRAY2_ERR_TOO_SMALL,
    PRAY2_ERR_MAGIC,
    PRAY2_ERR_VERSION,
    PRAY2_ERR_HEADER_SIZE,
    PRAY2_ERR_TABLE_RANGE,
    PRAY2_ERR_TABLE_SIZE,
    PRAY2_ERR_DUR_SIZE,
    PRAY2_ERR_DUR_RANGE
} pray2_status_t;

// Validates sizes/ranges, fills header struct & pointers. No CRC used.
static pray2_status_t pray2_validate_and_parse_no_crc(const uint8_t* buf, size_t len, pray2_header_t* out) {
    if (!buf || len < PRAY2_HEADER_SIZE) {
    print_uart("pray2 err: too small");
      
        return PRAY2_ERR_TOO_SMALL;
    
    }
    if (memcmp(buf, PRAY2_MAGIC, 5) != 0) {
       print_uart("pray2 err: magic");
        return PRAY2_ERR_MAGIC;
    }
    if (buf[5] != PRAY2_VERSION) {
      print_uart("pray2 err: version");   
        return PRAY2_ERR_VERSION;
    }
    uint16_t header_size = pray2_rd_u16le(buf + 6);
    if (header_size != PRAY2_HEADER_SIZE) {
      print_uart("pray2 err: header size");
        return PRAY2_ERR_HEADER_SIZE;
    }
    pray2_header_t h;
    h.year        = pray2_rd_u16le(buf + 8);
    h.days        = pray2_rd_u16le(buf + 10);
    h.start_month = buf[12];
    h.start_day   = buf[13];
    h.flags       = buf[14];
    h.method_code = buf[15];
    memcpy(h.rtc_ascii, buf + 16, 17);
    h.rtc_ascii[17] = '\0';

    const uint8_t* p = buf + 34;
    for (int i = 0; i < 5; ++i) h.default_on_sec[i] = pray2_rd_u16le(p + i*2);

    h.table_offset     = pray2_rd_u32le(buf + 44);
    h.table_size       = pray2_rd_u32le(buf + 48);
    h.durations_offset = pray2_rd_u32le(buf + 52);
    h.durations_size   = pray2_rd_u32le(buf + 56);

    // Basic sanity: table must lie within provided buffer (XMODEM padding may make len much larger).
    if (h.table_offset < PRAY2_HEADER_SIZE || h.table_offset > len) {
         print_uart("pray2 err: table_range");
        return PRAY2_ERR_TABLE_RANGE;
    }
    if (h.table_size != (uint32_t)h.days * 5u * 2u) {
       print_uart("pray2 err: table_size");
        return PRAY2_ERR_TABLE_SIZE;
    }
    if ((uint64_t)h.table_offset + (uint64_t)h.table_size > (uint64_t)len) {
         print_uart("pray2 err: table_range");
        return PRAY2_ERR_TABLE_RANGE;
    }
    if (h.flags & 0x01u) {
        if (h.durations_offset == 0 || h.durations_size != (uint32_t)h.days * 5u * 2u)
           {
             print_uart("pray2 err: dur_size");
              return PRAY2_ERR_DUR_SIZE;
           }
        if ((uint64_t)h.durations_offset + (uint64_t)h.durations_size > (uint64_t)len)
            {
                 print_uart("pray2 err: dur_range");
                return PRAY2_ERR_DUR_RANGE;
            }
    } else {
        if (h.durations_offset != 0 || h.durations_size != 0)
            {
                 print_uart("pray2 err: dur_range");
                return PRAY2_ERR_DUR_RANGE;
            }
    }

    h.table_ptr     = buf + h.table_offset;
    h.durations_ptr = (h.flags & 0x01u) ? (buf + h.durations_offset) : NULL;

    if (out) *out = h;
    return PRAY2_OK;
}

// Read one day's 5 times (minutes since local midnight). Returns false if out-of-range.
static bool pray2_get_day_minutes(const pray2_header_t* h, uint16_t day_index, uint16_t out_minutes[5]) {
    if (!h || !h->table_ptr || day_index >= h->days) return false;
    const uint8_t* rec = h->table_ptr + (size_t)day_index * 5u * 2u;
    for (int i = 0; i < 5; ++i) out_minutes[i] = pray2_rd_u16le(rec + i*2);
    return true;
}

// Compute day index (0..days-1) from a local Y/M/D, or -1 if outside span.
static int pray2_compute_day_index(const pray2_header_t* h, int year, int month, int day) {
    if (!h) return -1;
    int delta = pray2_days_between(h->year, h->start_month, h->start_day, year, month, day);
    if (delta < 0 || delta >= (int)h->days) return -1;
    return delta;
}

// ===== Scheduler context =====
typedef struct {
    bool           valid;        // parsed OK
    pray2_header_t H;            // header copy
    int            cur_day_idx;  // -1 if out of range / invalid
    uint16_t       today_min[5]; // Fajr..Isha (minutes since midnight)
    uint8_t        next_cursor;  // 0..5 (next prayer to watch)
    int            prev_min;     // last minutes since midnight (-1 initially)
} pray2_sched_t;

// Initialize scheduler from RAM blob + current RTC string.
// Returns true if valid & in-range; false if file invalid (scheduler will no-op).
static bool pray2_sched_init_from_ram(pray2_sched_t* ctx,
                                      const uint8_t* buf, size_t len,
                                      const char rtc_str17[17])
{
    if (!ctx) return false;
    memset(ctx, 0, sizeof(*ctx));
    ctx->prev_min = -1;

    pray2_status_t st = pray2_validate_and_parse_no_crc(buf, len, &ctx->H);
    if (st != PRAY2_OK) {
        ctx->valid = false;
        ctx->cur_day_idx = -1;
        print_uart("pray2 err: parse\r\n");
        return false;
    }
    ctx->valid = true;

    // If one-shot flag set, set RTC from header ascii and clear the flag in the stored blob.
    if (ctx->H.flags & PRAY2_FLAG_RTC_ONE_SHOT) {
        int hh, mm, ss, DD, MO, YYYY;
        if (pray2_parse_rtc_ascii(ctx->H.rtc_ascii, &hh,&mm,&ss,&DD,&MO,&YYYY)) {
            char setbuf[18];
            // Ensure slash format exactly: "HH:MM:SS|DD/MM/YY"
            snprintf(setbuf, sizeof(setbuf), "%02d:%02d:%02d|%02d/%02d/%02d",
                     hh, mm, ss, DD, MO, YYYY % 100);
            RTCmcp7940_set_datetime(RTC_MCP, setbuf);

            // Clear the flag in the RAM blob (header byte at offset 14)
            ((uint8_t*)buf)[14] = (uint8_t)(((uint8_t*)buf)[14] & ~(PRAY2_FLAG_RTC_ONE_SHOT));
            ctx->H.flags &= ~(PRAY2_FLAG_RTC_ONE_SHOT);

            print_uart("RTC set from file (one-shot) and flag cleared\r\n");
        } else {
            print_uart("pray2 warn: header RTC parse failed; skipping RTC set\r\n");
        }
    }

    // Use the actual RTC for scheduler init.
    char nowrtc[18] = {0};
    RTCmcp7940_get_datetime(RTC_MCP, nowrtc);  // "HH:MM:SS|DD/MM/YY"

    int hh, mm, ss, DD, MO, YYYY;
    if (!pray2_parse_rtc_ascii(nowrtc[0] ? nowrtc : rtc_str17, &hh,&mm,&ss,&DD,&MO,&YYYY)) {
        ctx->cur_day_idx = -1;
        print_uart("pray2 err: RTC ascii\r\n");
        return false;
    }
    const int now_min = hh * 60 + mm;

    const int idx = pray2_compute_day_index(&ctx->H, YYYY, MO, DD);
    ctx->cur_day_idx = idx;
    ctx->next_cursor = 5; // default (no upcoming)

    if (idx >= 0) {
        if (pray2_get_day_minutes(&ctx->H, (uint16_t)idx, ctx->today_min)) {
            // Choose the first prayer >= now
            uint8_t nc = 5;
            for (uint8_t i = 0; i < 5; ++i) {
                if ((int)ctx->today_min[i] >= now_min) { nc = i; break; }
            }
            ctx->next_cursor = nc;
        }
    }

    char localbuf[40];
    snprintf(localbuf, sizeof(localbuf), "pray2 IDX: %d\r\n", idx);
    print_uart(localbuf);

    ctx->prev_min = now_min;
    return (idx >= 0);
}

// 1 Hz tick. Returns true only when a prayer should fire *now*.
// On fire: *out_prayer is 0..4 (Fajr..Isha), *out_on_sec from header default_on_sec[*].
static bool pray2_sched_tick(pray2_sched_t* ctx,
                             const char rtc_str17[17],
                             int* out_prayer, uint16_t* out_on_sec)
{
    if (!ctx || !ctx->valid) return false;

    int hh, mm, ss, DD, MO, YYYY;
    if (!pray2_parse_rtc_ascii(rtc_str17, &hh,&mm,&ss,&DD,&MO,&YYYY)) return false;
    const int now_min = hh*60 + mm;

    // Day change?
    int idx = pray2_compute_day_index(&ctx->H, YYYY, MO, DD);
    if (idx != ctx->cur_day_idx) {
        ctx->cur_day_idx = idx;
        ctx->next_cursor = 5;
        if (idx >= 0 && pray2_get_day_minutes(&ctx->H, (uint16_t)idx, ctx->today_min)) {
            uint8_t nc = 5;
            for (uint8_t i = 0; i < 5; ++i) {
                if ((int)ctx->today_min[i] >= now_min) { nc = i; break; }
            }
            ctx->next_cursor = nc;
        }
        ctx->prev_min = now_min;
        return false; // do not fire on the exact minute of day rollover
    }

    // Minute edge?
    if (now_min == ctx->prev_min) return false;
    const int prev = ctx->prev_min;
    ctx->prev_min = now_min;

    if (ctx->cur_day_idx < 0 || ctx->next_cursor >= 5) return false;

    // POLICY A: if multiple events were skipped, fire only the earliest missed once.
    uint8_t i = ctx->next_cursor;
    if ((int)ctx->today_min[i] <= now_min) {
        // Fire if it is exactly now, or if it was missed in (prev..now].
        if ((int)ctx->today_min[i] > prev) {
            if (out_prayer)  *out_prayer = i;
            if (out_on_sec)  *out_on_sec = ctx->H.default_on_sec[i];
            ctx->next_cursor = (i+1u);
            return true;
        } else {
            // It was already <= prev (very large jump), advance cursor and do not fire now.
            while (ctx->next_cursor < 5 && (int)ctx->today_min[ctx->next_cursor] <= now_min) {
                ctx->next_cursor++;
            }
            return false;
        }
    }
    return false;
}




// ---- core dumper: print one specific YEAR+MONTH ----
static void debug_print_month_from_bin(const uint8_t *file_buf, size_t file_len,
                                int target_year, int target_month)
{
    char line[200];
    if (target_month < 1 || target_month > 12) {
        snprintf(line, sizeof(line), "Month %d invalid (1-12)\r\n", target_month);
        print_uart(line);
        return;
    }

    pray2_header_t H;
    pray2_status_t st = pray2_validate_and_parse_no_crc(file_buf, file_len, &H);
    if (st != PRAY2_OK) {
        snprintf(line, sizeof(line), "PRAY2 parse error %d\r\n", (int)st);
        print_uart(line);
        return;
    }

    int y = H.year, m = H.start_month, d = H.start_day;
    int printed = 0;

    snprintf(line, sizeof(line),
             "PRAY2 OK. SpanStart=%04d-%02d-%02d Days=%u  => Printing %04d-%02d\r\n",
             y, m, d, (unsigned)H.days, target_year, target_month);
    print_uart(line);

    for (uint32_t idx = 0; idx < H.days; ++idx) {
        if (y == target_year && m == target_month) {
            uint16_t mins[5];
            if (pray2_get_day_minutes(&H, (uint16_t)idx, mins)) {
                int Fh = mins[0]/60, Fm = mins[0]%60;
                int Dh = mins[1]/60, Dm = mins[1]%60;
                int Ah = mins[2]/60, Am = mins[2]%60;
                int Mh = mins[3]/60, Mm = mins[3]%60;
                int Ih = mins[4]/60, Im = mins[4]%60;
                snprintf(line, sizeof(line),
                         "%04d-%02d-%02d  Fajr %02d:%02d  Dhuhr %02d:%02d  Asr %02d:%02d  Maghrib %02d:%02d  Isha %02d:%02d\r\n",
                         y, m, d, Fh, Fm, Dh, Dm, Ah, Am, Mh, Mm, Ih, Im);
                print_uart(line);
                printed++;
            } else {
                snprintf(line, sizeof(line),
                         "%04d-%02d-%02d  ERROR: idx %lu\r\n",
                         y, m, d, (unsigned long)idx);
                print_uart(line);
            }
        }
        advance_one_day(&y, &m, &d);
    }

    if (printed == 0) {
        snprintf(line, sizeof(line),
                 "No dates for %04d-%02d within this file span.\r\n",
                 target_year, target_month);
        print_uart(line);
    } else {
        snprintf(line, sizeof(line),
                 "Printed %d day(s) for %04d-%02d.\r\n",
                 printed, target_year, target_month);
        print_uart(line);
    }
}

// ---- convenience wrapper: print ALL occurrences of a month across the span ----
static void debug_print_month_any_year(const uint8_t *file_buf, size_t file_len,
                                int target_month)
{
    char line[200];
    if (target_month < 1 || target_month > 12) {
        snprintf(line, sizeof(line), "Month %d invalid (1-12)\r\n", target_month);
        print_uart(line);
        return;
    }

    pray2_header_t H;
    pray2_status_t st = pray2_validate_and_parse_no_crc(file_buf, file_len, &H);
    if (st != PRAY2_OK) {
        snprintf(line, sizeof(line), "PRAY2 parse error %d\r\n", (int)st);
        print_uart(line);
        return;
    }

    int y = H.year, m = H.start_month, d = H.start_day;
    int printed = 0;

    snprintf(line, sizeof(line),
             "PRAY2 OK. SpanStart=%04d-%02d-%02d Days=%u  => Printing all %s (%02d)\r\n",
             y, m, d, (unsigned)H.days,
             (const char*[]){"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}[target_month-1],
             target_month);
    print_uart(line);

    for (uint32_t idx = 0; idx < H.days; ++idx) {
        if (m == target_month) {
            uint16_t mins[5];
            if (pray2_get_day_minutes(&H, (uint16_t)idx, mins)) {
                int Fh = mins[0]/60, Fm = mins[0]%60;
                int Dh = mins[1]/60, Dm = mins[1]%60;
                int Ah = mins[2]/60, Am = mins[2]%60;
                int Mh = mins[3]/60, Mm = mins[3]%60;
                int Ih = mins[4]/60, Im = mins[4]%60;
                snprintf(line, sizeof(line),
                         "%04d-%02d-%02d  Fajr %02d:%02d  Dhuhr %02d:%02d  Asr %02d:%02d  Maghrib %02d:%02d  Isha %02d:%02d\r\n",
                         y, m, d, Fh, Fm, Dh, Dm, Ah, Am, Mh, Mm, Ih, Im);
                print_uart(line);
                printed++;
            } else {
                snprintf(line, sizeof(line),
                         "%04d-%02d-%02d  ERROR: idx %lu\r\n",
                         y, m, d, (unsigned long)idx);
                
                         print_uart(line);
            }
        }
        advance_one_day(&y, &m, &d);
    }

    if (printed == 0) {
        snprintf(line, sizeof(line),
                 "No dates for month %02d within this file span.\r\n",
                 target_month);
        print_uart(line);
    } else {
        snprintf(line, sizeof(line),
                 "Printed %d day(s) for month %02d.\r\n",
                 printed, target_month);
        print_uart(line);
    }
}

#endif // PRAY2_SCHED_H
