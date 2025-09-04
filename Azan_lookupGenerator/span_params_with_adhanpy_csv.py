# span_params_with_adhanpy_csv.py
# Interactive span + parameters -> compute daily prayer times via adhanpy and write CSV.
# Follows adhanpy README: PrayerTimes(..., CalculationMethod.XXX, time_zone=ZoneInfo(...))

from __future__ import annotations
from datetime import datetime, date, timedelta
import calendar, csv, os

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

# ---------- span helpers ----------
def month_span(year: int, month: int) -> tuple[date, date]:
    last_day = calendar.monthrange(year, month)[1]
    return date(year, month, 1), date(year, month, last_day)

def contiguous_months_span(year: int, start_month: int, months: int) -> tuple[date, date]:
    end_month = min(12, start_month + months - 1)
    s, _ = month_span(year, start_month)
    _, e = month_span(year, end_month)
    return s, e

def daterange(start: date, end: date):
    d = start
    one = timedelta(days=1)
    while d <= end:
        yield d
        d += one

def count_days(start: date, end: date) -> int:
    return (end - start).days + 1

def human_months(start: date, end: date) -> str:
    if start.year == end.year and start.month == end.month:
        return f"{start.strftime('%b %Y')}"
    if start.year == end.year:
        return f"{start.strftime('%b')}–{end.strftime('%b %Y')}"
    return f"{start.strftime('%b %Y')} → {end.strftime('%b %Y')}"

# ---------- input helpers ----------
def ask_int(prompt: str, lo: int, hi: int) -> int:
    while True:
        try:
            v = int(input(f"{prompt} [{lo}-{hi}]: ").strip())
            if lo <= v <= hi:
                return v
        except Exception:
            pass
        print("  ✖ Invalid input, try again.")

def ask_float(prompt: str, lo: float, hi: float) -> float:
    while True:
        try:
            v = float(input(f"{prompt} [{lo}..{hi}]: ").strip())
            if lo <= v <= hi:
                return v
        except Exception:
            pass
        print("  ✖ Invalid input, try again.")

def ask_yes_no(prompt: str, default: bool = False) -> bool:
    suffix = "[Y/n]" if default else "[y/N]"
    while True:
        s = input(f"{prompt} {suffix}: ").strip().lower()
        if not s:
            return default
        if s in ("y", "yes"): return True
        if s in ("n", "no"):  return False
        print("  ✖ Please answer y or n.")

def ask_timezone() -> str:
    while True:
        tz = input("IANA time zone (e.g., Asia/Karachi, America/New_York): ").strip()
        try:
            ZoneInfo(tz)  # validation
            return tz
        except Exception:
            print("  ✖ Not a recognized IANA zone on this system. Try again.")

def choose_method() -> str:
    print("\nCalculation method:")
    for i, (key, desc) in enumerate(METHODS, 1):
        print(f"  {i}) {key:22s} – {desc}")
    idx = ask_int("Choose method", 1, len(METHODS))
    return METHODS[idx - 1][0]

def get_offsets() -> dict[str, int]:
    print("\nOptional per-prayer minute offsets (positive=later, negative=earlier).")
    print("Press Enter to accept 0.\n")
    offs: dict[str, int] = {}
    for p in PRAYERS:
        s = input(f"  {p} offset (min) [default 0]: ").strip()
        if not s:
            offs[p] = 0
        else:
            try: offs[p] = int(s)
            except Exception:
                print("   → Not a number; using 0.")
                offs[p] = 0
    return offs

def fmt(dt: datetime) -> str:
    return dt.strftime("%H:%M")

# ---------- CSV writer ----------
def default_csv_name(year: int, start: date, end: date, method_key: str) -> str:
    span = f"{start.strftime('%Y%m%d')}-{end.strftime('%Y%m%d')}"
    return f"prayer_times_{year}_{span}_{method_key}.csv"

def write_csv(path: str, start: date, end: date, lat: float, lon: float, tzname: str,
              method_key: str, offsets: dict[str, int]) -> None:
    tz = ZoneInfo(tzname)
    method_enum = METHOD_MAP[method_key]
    now_iso = datetime.now(ZoneInfo(tzname)).strftime("%Y-%m-%dT%H:%M:%S%z")

    # Windows Excel likes utf-8 with BOM
    with open(path, "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f)
        # Metadata header (comment lines)
        w.writerow([f"# Generated", now_iso])
        w.writerow([f"# Location", f"lat={lat:.6f}", f"lon={lon:.6f}"])
        w.writerow([f"# Timezone", tzname])
        w.writerow([f"# Method", method_key])
        w.writerow([f"# Offsets", *(f"{p}={offsets[p]}min" for p in PRAYERS)])
        # Column header
        w.writerow(["Date", "Weekday", "Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha"])

        for d in daterange(start, end):
            pt = PrayerTimes((lat, lon), datetime(d.year, d.month, d.day), method_enum, time_zone=tz)
            def adj(dt: datetime, mins: int) -> datetime: return dt + timedelta(minutes=mins)
            fajr    = adj(pt.fajr,    offsets["Fajr"])
            sunrise = pt.sunrise
            dhuhr   = adj(pt.dhuhr,   offsets["Dhuhr"])
            asr     = adj(pt.asr,     offsets["Asr"])
            maghrib = adj(pt.maghrib, offsets["Maghrib"])
            isha    = adj(pt.isha,    offsets["Isha"])
            w.writerow([
                d.isoformat(), d.strftime("%A"),
                *(t.strftime("%H:%M") for t in (fajr, sunrise, dhuhr, asr, maghrib, isha))
            ])

# ---------- main ----------
def main():
    print("=== Prayer Schedule (adhanpy) → CSV ===")
    year = ask_int("Year", 1900, 2100)

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
    choice = ask_int("Your choice", 1, 7)

    if choice == 1:
        start, end = date(year, 1, 1), date(year, 12, 31)
        span_label = "FULL YEAR"
    elif choice == 2:
        m = ask_int("Month number", 1, 12)
        start, end = month_span(year, m)
        span_label = f"MONTH {calendar.month_name[m]}"
    elif choice == 3:
        m = ask_int("Month number", 1, 12)
        last_day = calendar.monthrange(year, m)[1]
        d = ask_int("Day of month", 1, last_day)
        start = end = date(year, m, d)
        span_label = "SINGLE DAY"
    elif choice == 4:
        n = ask_int("How many months (first N)", 1, 12)
        start, end = contiguous_months_span(year, 1, n)
        span_label = f"FIRST {n} MONTHS"
    elif choice == 5:
        n = ask_int("How many months (last N)", 1, 12)
        start_month = max(1, 13 - n)
        start, end = contiguous_months_span(year, start_month, n)
        span_label = f"LAST {n} MONTHS"
    elif choice == 6:
        m = ask_int("Start month M", 1, 12)
        n = ask_int("How many months (N)", 1, 12)
        start, end = contiguous_months_span(year, m, n)
        span_label = f"{n} MONTHS starting {calendar.month_name[m]}"
    else:
        a = ask_int("Start month A", 1, 12)
        b = ask_int("End   month B", a, 12)
        start, end = contiguous_months_span(year, a, b - a + 1)
        span_label = f"MONTHS {calendar.month_name[a]}–{calendar.month_name[b]}"

    print("\nLocation (installation city):")
    lat = ask_float("Latitude", -90.0, 90.0)
    lon = ask_float("Longitude", -180.0, 180.0)
    tzname = ask_timezone()

    method_key = choose_method()
    use_offsets = ask_yes_no("\nApply simple per-prayer offsets?", default=False)
    offsets = get_offsets() if use_offsets else {p: 0 for p in PRAYERS}

    # Summary
    print("\n=== SUMMARY ===")
    print(f"Span     : {span_label}")
    print(f"Start    : {start.isoformat()}")
    print(f"End      : {end.isoformat()}")
    print(f"Days     : {count_days(start, end)}")
    print(f"Months   : {human_months(start, end)}")
    print(f"Location : lat={lat:.6f}, lon={lon:.6f}")
    print(f"Timezone : {tzname}")
    print(f"Method   : {method_key}")
    print("Offsets  :", ", ".join(f"{p}={offsets[p]} min" for p in PRAYERS))

    # CSV output path
    default_name = default_csv_name(year, start, end, method_key)
    out_name = input(f"\nCSV file name [default {default_name}]: ").strip() or default_name
    out_path = os.path.abspath(out_name)

    print("\n=== GENERATING CSV ===")
    write_csv(out_path, start, end, lat, lon, tzname, method_key, offsets)
    print(f"CSV written: {out_path}")

if __name__ == "__main__":
    main()
