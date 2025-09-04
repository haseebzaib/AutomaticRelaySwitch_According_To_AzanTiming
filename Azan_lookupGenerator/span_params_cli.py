# span_params_cli.py
# Interactive selector for date span + prayer-time parameters (NO timing math yet).
# It only collects inputs and prints exactly what would be generated.

from __future__ import annotations
from datetime import date
import calendar

try:
    from zoneinfo import ZoneInfo  # for validating IANA timezone names
except Exception:
    ZoneInfo = None  # We'll accept any string if ZoneInfo is unavailable


# ---------- date-span helpers (same-year) ----------
def month_span(year: int, month: int) -> tuple[date, date]:
    last_day = calendar.monthrange(year, month)[1]
    return date(year, month, 1), date(year, month, last_day)

def contiguous_months_span(year: int, start_month: int, months: int) -> tuple[date, date]:
    end_month = min(12, start_month + months - 1)
    start, _ = month_span(year, start_month)
    _, end = month_span(year, end_month)
    return start, end

def count_days(start: date, end: date) -> int:
    return (end - start).days + 1

def human_months(start: date, end: date) -> str:
    if start.year == end.year and start.month == end.month:
        return f"{start.strftime('%b %Y')}"
    if start.year == end.year:
        return f"{start.strftime('%b')}–{end.strftime('%b %Y')}"
    return f"{start.strftime('%b %Y')} \u2192 {end.strftime('%b %Y')}"


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
        if not tz:
            print("  ✖ Please enter a non-empty timezone string.")
            continue
        if ZoneInfo is None:
            return tz  # cannot validate here
        try:
            ZoneInfo(tz)
            return tz
        except Exception:
            print("  ✖ Not a recognized IANA zone on this system. Try again.")


# ---------- parameter pickers ----------
METHODS = [
    # Names mirror typical Adhan method families (we’ll map to adhanpy enums later).
    ("KARACHI", "University of Islamic Sciences, Karachi (Fajr 18°, Isha 18°)"),
    ("MUSLIM_WORLD_LEAGUE", "MWL (Fajr 18°, Isha 17°)"),
    ("EGYPTIAN", "Egyptian General Authority (Fajr 19.5°, Isha 17.5°)"),
    ("MOON_SIGHTING_COMMITTEE", "Moonsighting Committee"),
    ("UMM_AL_QURA", "Umm al-Qurā (Isha = Maghrib + 90; often 120 in Ramadan)"),
    ("NORTH_AMERICA", "ISNA/North America (Fajr 15°, Isha 15°)"),
]

MADHABS = [
    ("SHAFI",  "Asr when shadow = 1× object length"),
    ("HANAFI", "Asr when shadow = 2× object length"),
]

PRAYERS = ["Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"]

def choose_method() -> str:
    print("\nCalculation method:")
    for i, (key, desc) in enumerate(METHODS, 1):
        print(f"  {i}) {key:22s} – {desc}")
    idx = ask_int("Choose method", 1, len(METHODS))
    return METHODS[idx - 1][0]

def choose_madhab() -> str:
    print("\nAsr juristic method (Madhab):")
    for i, (key, desc) in enumerate(MADHABS, 1):
        print(f"  {i}) {key:6s} – {desc}")
    idx = ask_int("Choose madhab", 1, len(MADHABS))
    return MADHABS[idx - 1][0]

def get_offsets() -> dict[str, int]:
    print("\nOptional per-prayer minute offsets (positive=later, negative=earlier).")
    print("Press Enter to accept 0.\n")
    offs = {}
    for p in PRAYERS:
        s = input(f"  {p} offset (min) [default 0]: ").strip()
        if not s:
            offs[p] = 0
        else:
            try:
                offs[p] = int(s)
            except Exception:
                print("   → Not a number; using 0.")
                offs[p] = 0
    return offs


# ---------- main flow ----------
def main():
    print("=== Prayer Schedule Parameter Selector (no timings yet) ===")
    year = ask_int("Year", 1900, 2100)

    # Date-span choice
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

    # Location & timezone
    print("\nLocation (installation city):")
    lat = ask_float("Latitude", -90.0, 90.0)
    lon = ask_float("Longitude", -180.0, 180.0)
    tz  = ask_timezone()

    # Method & madhab
    method = choose_method()
    madhab = choose_madhab()

    # Simple offsets (Tier 1)
    use_offsets = ask_yes_no("\nApply simple per-prayer offsets?", default=False)
    offsets = get_offsets() if use_offsets else {p: 0 for p in PRAYERS}

    # Summary (no computation yet)
    print("\n=== SUMMARY (no timings computed) ===")
    print(f"Span     : {span_label}")
    print(f"Start    : {start.isoformat()}")
    print(f"End      : {end.isoformat()}")
    print(f"Days     : {count_days(start, end)}")
    print(f"Months   : {human_months(start, end)}")
    print(f"Location : lat={lat:.6f}, lon={lon:.6f}")
    print(f"Timezone : {tz}")
    print(f"Method   : {method}")
    print(f"Madhab   : {madhab}")
    print("Offsets  :", ", ".join(f"{p}={offsets[p]} min" for p in PRAYERS))
 

if __name__ == "__main__":
    main()
