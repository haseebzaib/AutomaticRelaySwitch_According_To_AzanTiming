// pray2_tests.c — runtime tests for PRAY2 scheduler (prints via print_uart)

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "pray2_reader.h"    // parser + scheduler (no CRC)
#include "RTCmcp7940.h"

// Provided by you:
extern void print_uart(char *buf);
extern uint8_t  DataBuffer[];        // whole .bin in RAM (+ optional padding)
extern uint16_t   DataBufferTotalSize; // length of RAM buffer
extern const struct device *RTC_MCP;

static const char* PRAYER_NAME[5] = {"Fajr","Dhuhr","Asr","Maghrib","Isha"};



// Build "HH:MM:SS|DD/MM/YY" into out[18] (NUL-terminated)
static void make_rtc_str(int Y, int M, int D, int hh, int mm, int ss, char out[18]) {
    snprintf(out, 18, "%02d:%02d:%02d|%02d/%02d/%02d", hh, mm, ss, D, M, (Y % 100));
    out[17] = '\0';
}

// Pretty printer for one day’s times
static void print_day_line(int Y,int M,int D, const uint16_t mins[5]) {
    char line[200];
    snprintf(line, sizeof(line),
             "%04d-%02d-%02d  Fajr %02d:%02d  Dhuhr %02d:%02d  Asr %02d:%02d  Maghrib %02d:%02d  Isha %02d:%02d\r\n",
             Y,M,D,
             mins[0]/60, mins[0]%60, mins[1]/60, mins[1]%60, mins[2]/60, mins[2]%60,
             mins[3]/60, mins[3]%60, mins[4]/60, mins[4]%60);
    print_uart(line);
}

// Initialize sched at a specific simulated time/date
static bool sched_set_time(pray2_sched_t* s, const uint8_t* file, size_t len,
                           int Y,int M,int D,int hh,int mm,int ss) {
    char rtc[18];
    make_rtc_str(Y,M,D, hh,mm,ss, rtc);
    return pray2_sched_init_from_ram(s, file, len, rtc);
}

// One tick at a specific simulated minute, print if fires
static bool sched_tick_at(pray2_sched_t* s, int Y,int M,int D,int hh,int mm) {
    char rtc[18];
    make_rtc_str(Y,M,D, hh,mm,0, rtc);
    int prayer; uint16_t sec;
    if (pray2_sched_tick(s, rtc, &prayer, &sec)) {
        char line[128];
        snprintf(line, sizeof(line), "FIRE  %s at %02d:%02d  ON=%us\r\n", PRAYER_NAME[prayer], hh, mm, (unsigned)sec);
        print_uart(line);
        // Optionally drive the relay here:
        // relay_on_for_seconds(sec);
        return true;
    }
    return false;
}

// ---- TEST 1: quick-fire each prayer (T-1 minute -> hit) ----
static void test_quick_fire_each(pray2_sched_t* s, const pray2_header_t* H,
                                 const uint8_t* file, size_t len,
                                 int Y,int M,int D)
{
    char line[160];
    int idx = pray2_compute_day_index(H, Y, M, D);
    if (idx < 0) { print_uart("T1: date out of span\r\n"); return; }

    uint16_t mins[5];
    pray2_get_day_minutes(H, (uint16_t)idx, mins);
    print_uart("T1: Quick-fire each prayer (T-1min then hit):\r\n");
    print_day_line(Y,M,D, mins);

    for (int p = 0; p < 5; ++p) {
        int hh = mins[p]/60, mm = mins[p]%60;
        // set to one minute before (wrap if needed)
        if (mm == 0) { mm = 59; hh = (hh + 23) % 24; } else { mm -= 1; }
        if (!sched_set_time(s, file, len, Y,M,D, hh,mm,0)) {
            print_uart("  init failed\r\n"); return;
        }
        // advance 1 minute -> should fire exactly once
        int nh = (mins[p]/60), nm = (mins[p]%60);
        bool fired = sched_tick_at(s, Y,M,D, nh,nm);
        snprintf(line, sizeof(line), "  Expect %s at %02d:%02d -> %s\r\n",
                 PRAYER_NAME[p], nh, nm, fired ? "OK" : "MISS");
        print_uart(line);
    }
}

// ---- TEST 2: full-day sweep (00:00 -> 23:59) ----
static void test_full_day_sweep(pray2_sched_t* s, const pray2_header_t* H,
                                const uint8_t* file, size_t len,
                                int Y,int M,int D)
{
    char line[160];
    int fires = 0;
    print_uart("T2: Full-day sweep 00:00->23:59\r\n");
    (void)sched_set_time(s, file, len, Y,M,D, 0,0,0);
    for (int m = 0; m < 24*60; ++m) {
        int hh = m/60, mm = m%60;
        if (sched_tick_at(s, Y,M,D, hh,mm)) fires++;
    }
    snprintf(line, sizeof(line), "  Total fires: %d (expect 5)\r\n", fires);
    print_uart(line);
}

// ---- TEST 3: day rollover (last 5 min -> next day 10 min) ----
static void test_day_rollover(pray2_sched_t* s, const pray2_header_t* H,
                              const uint8_t* file, size_t len,
                              int Y,int M,int D)
{
    char line[120];
    print_uart("T3: Day rollover (23:55..23:59 -> next day 00:00..00:09)\r\n");
    (void)sched_set_time(s, file, len, Y,M,D, 23,55,0);
    for (int m = 23*60+55; m < 24*60; ++m) {
        int hh=m/60, mm=m%60; (void)sched_tick_at(s, Y,M,D, hh,mm);
    }
    // next day
    int Y2=Y, M2=M, D2=D; advance_one_day(&Y2,&M2,&D2);
    for (int m = 0; m < 10; ++m) {
        int hh=m/60, mm=m%60; (void)sched_tick_at(s, Y2,M2,D2, hh,mm);
    }
    snprintf(line, sizeof(line), "  Rolled to %04d-%02d-%02d\r\n", Y2,M2,D2);
    print_uart(line);
}

// ---- TEST 4: clock jump forward (Policy A: fire earliest missed only) ----
static void test_clock_jump_forward(pray2_sched_t* s, const pray2_header_t* H,
                                    const uint8_t* file, size_t len,
                                    int Y,int M,int D)
{
    char line[200];
    int idx = pray2_compute_day_index(H, Y, M, D);
    if (idx < 0) { print_uart("T4: date out of span\r\n"); return; }
    uint16_t mins[5]; pray2_get_day_minutes(H, (uint16_t)idx, mins);
    print_uart("T4: Clock jump forward (+several hours) -> earliest missed only\r\n");
    print_day_line(Y,M,D, mins);

    // Start 10 minutes before Dhuhr
    int start_min = mins[1] - 10;
    int sh = start_min/60, sm = start_min%60;
    (void)sched_set_time(s, file, len, Y,M,D, sh,sm,0);

    // Jump forward to Isha+1 minute
    int jump_min = mins[4] + 1; if (jump_min > 23*60+59) jump_min = 23*60+59;
    int jh = jump_min/60, jm = jump_min%60;
    bool fired = sched_tick_at(s, Y,M,D, jh,jm);
    snprintf(line, sizeof(line), "  Jump to %02d:%02d -> %s (should be Dhuhr only)\r\n",
             jh, jm, fired ? "FIRE" : "no fire (no event in gap)");
    print_uart(line);
}

// ---- choose a good in-span date (mid-span) ----
static void pick_mid_span_date(const pray2_header_t* H, int* Y,int* M,int* D) {
    int y = H->year, m = H->start_month, d = H->start_day;
    uint32_t steps = (H->days > 1) ? (H->days/2) : 0;
    for (uint32_t i=0;i<steps;i++) advance_one_day(&y,&m,&d);
    *Y=y; *M=m; *D=d;
}


void run_pray2_tests(void)
{
    char line[160];
    pray2_header_t H;
    pray2_status_t st = pray2_validate_and_parse_no_crc(DataBuffer, DataBufferTotalSize, &H);
    if (st != PRAY2_OK) {
        snprintf(line, sizeof(line), "PRAY2 parse error %d\r\n", (int)st);
        print_uart(line);
        return;
    }

    int Y,M,D; pick_mid_span_date(&H, &Y,&M,&D);
    snprintf(line, sizeof(line),
             "TESTS on %04d-%02d-%02d  (SpanStart=%04d-%02d-%02d Days=%u)\r\n",
             Y,M,D, H.year,H.start_month,H.start_day, (unsigned)H.days);
    print_uart(line);

    // Show the chosen day
    uint16_t mins[5]; int idx = pray2_compute_day_index(&H, Y,M,D);
    if (idx >= 0 && pray2_get_day_minutes(&H, (uint16_t)idx, mins)) {
        print_day_line(Y,M,D, mins);
    }

    static pray2_sched_t sched;

    test_quick_fire_each(&sched, &H, DataBuffer, DataBufferTotalSize, Y,M,D);
    test_full_day_sweep(&sched, &H, DataBuffer, DataBufferTotalSize, Y,M,D);
    test_day_rollover(&sched, &H, DataBuffer, DataBufferTotalSize, Y,M,D);
    test_clock_jump_forward(&sched, &H, DataBuffer, DataBufferTotalSize, Y,M,D);

    print_uart("All tests done.\r\n");
}
