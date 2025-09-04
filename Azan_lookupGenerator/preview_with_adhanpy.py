# preview_with_adhanpy.py (no Madhab usage; adhanpy-verified)
from datetime import datetime, date, timedelta
import calendar
from zoneinfo import ZoneInfo

from adhanpy.PrayerTimes import PrayerTimes
from adhanpy.calculation import CalculationMethod

def month_span(year: int, month: int):
    last_day = calendar.monthrange(year, month)[1]
    return date(year, month, 1), date(year, month, last_day)

def daterange(start: date, end: date):
    d = start
    one = timedelta(days=1)
    while d <= end:
        yield d
        d += one

# ---- CONFIGURE HERE ----
LAT, LON = 33.6844, 73.0479           # Islamabad
TZ_NAME   = "Asia/Karachi"
METHOD    = CalculationMethod.KARACHI  # or: MUSLIM_WORLD_LEAGUE, MOON_SIGHTING_COMMITTEE, etc.

START, END = date(2025, 8, 28), date(2025, 8, 31)

# Optional simple offsets (minutes): positive=later, negative=earlier
OFFSETS = {"fajr": 0, "dhuhr": 0, "asr": 0, "maghrib": 0, "isha": 0}
# e.g. to test your observation: OFFSETS["isha"] = 15

# ------------------------

def compute_for_day(d: date, tz: ZoneInfo):
    pt = PrayerTimes(
        (LAT, LON),
        datetime(d.year, d.month, d.day),
        METHOD,
        time_zone=tz,  # ensures local-time outputs per README
    )
    def adj(dt: datetime, mins: int) -> datetime:
        return dt + timedelta(minutes=mins)
    return {
        "fajr":    adj(pt.fajr,    OFFSETS["fajr"]),
        "sunrise": pt.sunrise,
        "dhuhr":   adj(pt.dhuhr,   OFFSETS["dhuhr"]),
        "asr":     adj(pt.asr,     OFFSETS["asr"]),
        "maghrib": adj(pt.maghrib, OFFSETS["maghrib"]),
        "isha":    adj(pt.isha,    OFFSETS["isha"]),
    }

def fmt(dt: datetime): return dt.strftime("%H:%M")

def main():
    tz = ZoneInfo(TZ_NAME)
    print(f"=== Preview {START.isoformat()} → {END.isoformat()} | {TZ_NAME} ===")
    print(f"Method={METHOD.name}, Offsets={OFFSETS}\n")
    for d in daterange(START, END):
        t = compute_for_day(d, tz)
        print(f"{d.strftime('%A %Y-%m-%d')}:")
        print(f"  Fajr    {fmt(t['fajr'])}")
        print(f"  Sunrise {fmt(t['sunrise'])}")
        print(f"  Dhuhr   {fmt(t['dhuhr'])}")
        print(f"  Asr     {fmt(t['asr'])}")
        print(f"  Maghrib {fmt(t['maghrib'])}")
        print(f"  Isha    {fmt(t['isha'])}\n")

if __name__ == "__main__":
    main()
