# generate_pray2_bin_and_csv.py
# One interactive tool: computes times with adhanpy, writes PRAY2 .bin and (optionally) CSV.

from __future__ import annotations
from datetime import datetime, date, timedelta
import calendar, os, struct, zlib, csv
from zoneinfo import ZoneInfo
from adhanpy.PrayerTimes import PrayerTimes
from adhanpy.calculation import CalculationMethod

PRAYERS = ["Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"]

METHODS = [
    ("KARACHI", "University of Islamic Sciences, Karachi (Fajr 18°, Isha 18°)"),
    ("MUSLIM_WORLD_LEAGUE", "MWL (Fajr 18°, Isha 17°)"),
    ("EGYPTIAN", "Egyptian General Authority (Fajr 19.5°, Isha 17.5°)"),
    ("MOON_SIGHTING_COMMITTEE", "Moonsighting Committee"),
    ("UMM_AL_QURA", "Umm al-Qurā (Isha = Maghrib + 90; often 120 in Ramadan)"),
    ("NORTH_AMERICA", "ISNA/North America (Fajr 15°, Isha 15°)"),
]
METHOD_MAP = {
    "KARACHI": CalculationMethod.KARACHI,
    "MUSLIM_WORLD_LEAGUE": CalculationMethod.MUSLIM_WORLD_LEAGUE,
    "EGYPTIAN": CalculationMethod.EGYPTIAN,
    "MOON_SIGHTING_COMMITTEE": CalculationMethod.MOON_SIGHTING_COMMITTEE,
    "UMM_AL_QURA": CalculationMethod.UMM_AL_QURA,
    "NORTH_AMERICA": CalculationMethod.NORTH_AMERICA,
}
METHOD_CODE = {
    "CUSTOM": 0, "KARACHI": 1, "MUSLIM_WORLD_LEAGUE": 2, "EGYPTIAN": 3,
    "UMM_AL_QURA": 4, "MOON_SIGHTING_COMMITTEE": 5, "NORTH_AMERICA": 6,
}

# One-shot RTC flag (MCU will clear this after setting its RTC once)
FLAG_RTC_ONE_SHOT = 0x10  # bit 4

# ---- helpers ----
def validate_rtc_ascii(s: str) -> str | None:
    """
    Verify 'HH:MM:SS|DD/MM/YY' exactly (17 chars) and return normalized string or None.
    """
    s = s.strip()
    if len(s) != 17:
        return None
    try:
        if not (s[2]==':' and s[5]==':' and s[8]=='|' and s[11]=='/' and s[14]=='/'):
            return None
        HH = int(s[0:2]); MM = int(s[3:5]); SS = int(s[6:8])
        DD = int(s[9:11]); MO = int(s[12:14]); YY = int(s[15:17])
        if not (0 <= HH <= 23 and 0 <= MM <= 59 and 0 <= SS <= 59):
            return None
        if not (1 <= MO <= 12 and 1 <= DD <= 31):
            return None
        # Return in exact normalized format
        return f"{HH:02d}:{MM:02d}:{SS:02d}|{DD:02d}/{MO:02d}/{YY:02d}"
    except Exception:
        return None

def ask_rtc_ascii(tzname: str) -> str:
    """
    Ask user to type installation-local RTC time. Enter to use computer's current time in tzname.
    """
    default = datetime.now(ZoneInfo(tzname)).strftime("%H:%M:%S|%d/%m/%y")
    print(f"\nEnter the device RTC time to embed (local to installation).")
    print(f"Format: HH:MM:SS|DD/MM/YY   e.g. {default}")
    print(f"Press Enter to use computer's current local time for {tzname}: {default}")
    while True:
        s = input("RTC time: ").strip()
        if not s:
            return default
        v = validate_rtc_ascii(s)
        if v is not None:
            return v
        print("  ✖ Invalid format. Please use exactly HH:MM:SS|DD/MM/YY (17 chars). Try again.")


def month_span(year:int, month:int):
    last = calendar.monthrange(year, month)[1]
    return date(year, month, 1), date(year, month, last)
def contiguous_months_span(year:int, start_month:int, months:int):
    end_month = min(12, start_month+months-1)
    s,_ = month_span(year, start_month); _,e = month_span(year, end_month); return s,e
def daterange(start:date, end:date):
    d=start; one=timedelta(days=1)
    while d<=end: yield d; d+=one
def count_days(start:date, end:date): return (end-start).days+1
def human_months(start:date, end:date):
    if start.year==end.year and start.month==end.month: return start.strftime('%b %Y')
    if start.year==end.year: return f"{start.strftime('%b')}–{end.strftime('%b %Y')}"
    return f"{start.strftime('%b %Y')} → {end.strftime('%b %Y')}"
def ask_int(p,lo,hi):
    while True:
        try:
            v=int(input(f"{p} [{lo}-{hi}]: ").strip()); 
            if lo<=v<=hi: return v
        except: pass
        print("  ✖ Invalid input, try again.")
def ask_float(p,lo,hi):
    while True:
        try:
            v=float(input(f"{p} [{lo}..{hi}]: ").strip()); 
            if lo<=v<=hi: return v
        except: pass
        print("  ✖ Invalid input, try again.")
def ask_yes_no(p, default=False):
    suf = "[Y/n]" if default else "[y/N]"
    while True:
        s=input(f"{p} {suf}: ").strip().lower()
        if not s: return default
        if s in ("y","yes"): return True
        if s in ("n","no"): return False
        print("  ✖ Please answer y or n.")
def ask_timezone():
    while True:
        tz=input("IANA time zone (e.g., Asia/Karachi, America/New_York): ").strip()
        try: ZoneInfo(tz); return tz
        except: print("  ✖ Not a recognized IANA zone. Try again.")
def choose_method():
    print("\nCalculation method:")
    for i,(k,desc) in enumerate(METHODS,1): print(f"  {i}) {k:22s} – {desc}")
    idx=ask_int("Choose method",1,len(METHODS)); return METHODS[idx-1][0]
def get_offsets():
    print("\nOptional per-prayer minute offsets (positive=later, negative=earlier).")
    print("Press Enter to accept 0.\n")
    offs={}
    for p in PRAYERS:
        s=input(f"  {p} offset (min) [default 0]: ").strip()
        if not s: offs[p]=0
        else:
            try: offs[p]=int(s)
            except: print("   → Not a number; using 0."); offs[p]=0
    return offs
def get_default_durations():
    print("\nDefault relay ON durations (seconds) for Fajr, Dhuhr, Asr, Maghrib, Isha.")
    print("Press Enter for defaults: 60,45,45,45,45")
    defaults=[60,45,45,45,45]; out=[]
    for i,p in enumerate(PRAYERS):
        s=input(f"  {p} seconds [default {defaults[i]}]: ").strip()
        if not s: out.append(defaults[i])
        else:
            try:
                v=int(s); 
                if v<0 or v>36000: raise ValueError()
                out.append(v)
            except: print("   → Invalid; using default."); out.append(defaults[i])
    return out

# ---- compute table (minutes from local midnight for 5 prayers) ----
def compute_minutes_table(start,end,lat,lon,tzname,method_key,offsets):
    tz=ZoneInfo(tzname); method_enum=METHOD_MAP[method_key]; rows=[]
    for d in daterange(start,end):
        pt=PrayerTimes((lat,lon), datetime(d.year,d.month,d.day), method_enum, time_zone=tz)
        adj=lambda dt,mins: dt+timedelta(minutes=mins)
        fajr=adj(pt.fajr,offsets["Fajr"]); dhuhr=adj(pt.dhuhr,offsets["Dhuhr"])
        asr=adj(pt.asr,offsets["Asr"]); mag=adj(pt.maghrib,offsets["Maghrib"])
        isha=adj(pt.isha,offsets["Isha"])
        to_min=lambda dt: dt.hour*60+dt.minute
        t=[to_min(fajr), to_min(dhuhr), to_min(asr), to_min(mag), to_min(isha)]
        for i in range(5): t[i]=max(0,min(1439,t[i]))
        for i in range(1,5):
            if t[i]<=t[i-1]: t[i]=min(t[i-1]+1,1439)
        rows.append(tuple(t))
    return rows

# ---- CSV writer (includes Sunrise for human check) ----
def write_csv(path,start,end,lat,lon,tzname,method_key,offsets):
    tz=ZoneInfo(tzname); method_enum=METHOD_MAP[method_key]
    now_iso=datetime.now(ZoneInfo(tzname)).strftime("%Y-%m-%dT%H:%M:%S%z")
    with open(path,"w",newline="",encoding="utf-8-sig") as f:
        w=csv.writer(f)
        w.writerow(["# Generated", now_iso])
        w.writerow(["# Location", f"lat={lat:.6f}", f"lon={lon:.6f}"])
        w.writerow(["# Timezone", tzname])
        w.writerow(["# Method", method_key])
        w.writerow(["# Offsets", *(f"{p}={offsets[p]}min" for p in PRAYERS)])
        w.writerow(["Date","Weekday","Fajr","Sunrise","Dhuhr","Asr","Maghrib","Isha"])
        for d in daterange(start,end):
            pt=PrayerTimes((lat,lon), datetime(d.year,d.month,d.day), method_enum, time_zone=tz)
            adj=lambda dt,mins: dt+timedelta(minutes=mins)
            fajr=adj(pt.fajr,offsets["Fajr"]); sunrise=pt.sunrise
            dhuhr=adj(pt.dhuhr,offsets["Dhuhr"]); asr=adj(pt.asr,offsets["Asr"])
            mag=adj(pt.maghrib,offsets["Maghrib"]); isha=adj(pt.isha,offsets["Isha"])
            w.writerow([d.isoformat(), d.strftime("%A"),
                        *(t.strftime("%H:%M") for t in (fajr,sunrise,dhuhr,asr,mag,isha))])

# ---- PRAY2 v2 BIN writer (local RTC string) ----
def write_pray2_bin(path, start, end, lat, lon, tzname, method_key, offsets, default_on,
                    rtc_ascii: str, flags: int):
    rows = compute_minutes_table(start, end, lat, lon, tzname, method_key, offsets)
    days = len(rows); start_month = start.month; start_day = start.day; year = start.year
    magic = b"PRAY2"; version = 2; header_size = 64
    method_code = METHOD_CODE[method_key]

    rtc_bytes = rtc_ascii.encode("ascii")
    if len(rtc_bytes) != 17:
        raise RuntimeError(f"RTC string must be 17 chars, got {len(rtc_bytes)}")

    table_offset = header_size
    table_size = days * 5 * 2
    durations_offset = 0
    durations_size = 0

    buf = bytearray()
    buf += magic
    buf += struct.pack("<B", version)
    buf += struct.pack("<H", header_size)
    buf += struct.pack("<H", year)
    buf += struct.pack("<H", days)
    buf += struct.pack("<B", start_month)
    buf += struct.pack("<B", start_day)
    buf += struct.pack("<B", flags & 0xFF)       # <-- flags includes RTC_ONE_SHOT bit if chosen
    buf += struct.pack("<B", method_code)
    buf += rtc_bytes                               # 17
    buf += struct.pack("<B", 0)                    # pad to 34
    buf += struct.pack("<5H", *default_on)         # 34..43
    buf += struct.pack("<I", table_offset)
    buf += struct.pack("<I", table_size)
    buf += struct.pack("<I", durations_offset)
    buf += struct.pack("<I", durations_size)
    buf += struct.pack("<H", 0)
    buf += struct.pack("<H", 0)
    assert len(buf) == header_size

    for t in rows:
        buf += struct.pack("<5H", *t)

    # Keep CRC appended (MCU ignores it; harmless). Delete these 2 lines if you truly don't want it.
    crc = zlib.crc32(buf) & 0xFFFFFFFF
    buf += struct.pack("<I", crc)

    with open(path, "wb") as f:
        f.write(buf)

# ---- main ----
def main():
    print("=== Prayer Schedule → PRAY2 .bin (+ optional CSV) ===")
    year=ask_int("Year",1900,2100)
    print("""
Choose what to generate:
  1) Full year
  2) Single month
  3) Single day
  4) First N months of the year
  5) Last  N months of the year
  6) From month M for N months (contiguous)
  7) From month A to month B (inclusive, same year)
""")
    choice=ask_int("Your choice",1,7)
    if choice==1: start,end=date(year,1,1),date(year,12,31); span_label="FULL YEAR"
    elif choice==2:
        m=ask_int("Month number",1,12); start,end=month_span(year,m); span_label=f"MONTH {calendar.month_name[m]}"
    elif choice==3:
        m=ask_int("Month number",1,12); last=calendar.monthrange(year,m)[1]; d=ask_int("Day of month",1,last)
        start=end=date(year,m,d); span_label="SINGLE DAY"
    elif choice==4:
        n=ask_int("How many months (first N)",1,12); start,end=contiguous_months_span(year,1,n); span_label=f"FIRST {n} MONTHS"
    elif choice==5:
        n=ask_int("How many months (last N)",1,12); sm=max(1,13-n); start,end=contiguous_months_span(year,sm,n); span_label=f"LAST {n} MONTHS"
    elif choice==6:
        m=ask_int("Start month M",1,12); n=ask_int("How many months (N)",1,12)
        start,end=contiguous_months_span(year,m,n); span_label=f"{n} MONTHS starting {calendar.month_name[m]}"
    else:
        a=ask_int("Start month A",1,12); b=ask_int("End   month B",a,12)
        start,end=contiguous_months_span(year,a,b-a+1); span_label=f"MONTHS {calendar.month_name[a]}–{calendar.month_name[b]}"

    print("\nLocation (installation city):")
    lat=ask_float("Latitude",-90.0,90.0)
    lon=ask_float("Longitude",-180.0,180.0)
    tzname=ask_timezone()
    method_key=choose_method()
    use_offsets=ask_yes_no("\nApply simple per-prayer offsets?", default=False)
    offsets=get_offsets() if use_offsets else {p:0 for p in PRAYERS}
    print("\nDefault relay durations:")
    default_on=get_default_durations()
       # --- Manual RTC entry + one-shot flag ---
    rtc_ascii = ask_rtc_ascii(tzname)   # user-typed or defaulted to current time in tzname
    set_once = ask_yes_no("Set RTC on device once from this file?", default=True)
    flags = FLAG_RTC_ONE_SHOT if set_once else 0

    span=f"{start.strftime('%Y%m%d')}-{end.strftime('%Y%m%d')}"
    bin_default=f"prayer_{year}_{span}_{method_key}.bin"
    bin_name=input(f"\nBIN file name [default {bin_default}]: ").strip() or bin_default
    bin_path=os.path.abspath(bin_name)

    print("\n=== Writing BIN ===")
    write_pray2_bin(bin_path, start, end, lat, lon, tzname, method_key, offsets,
                    default_on, rtc_ascii, flags)
    size=os.path.getsize(bin_path)
    print(f"BIN written: {bin_path}  |  size: {size} bytes ({size/1024:.2f} KiB)")

    if ask_yes_no("\nAlso write a CSV for verification?", default=True):
        csv_default=f"prayer_times_{year}_{span}_{method_key}.csv"
        csv_name=input(f"CSV file name [default {csv_default}]: ").strip() or csv_default
        csv_path=os.path.abspath(csv_name)
        print("=== Writing CSV ===")
        write_csv(csv_path,start,end,lat,lon,tzname,method_key,offsets)
        print(f"CSV written: {csv_path}")

    print("\nDone.")

if __name__=="__main__":
    main()
