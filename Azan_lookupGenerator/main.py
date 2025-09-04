from datetime import datetime
from zoneinfo import ZoneInfo
from adhanpy.calculation import CalculationMethod
from adhanpy.PrayerTimes import PrayerTimes

def show_for_dates(dates, coords, method, tz):
    fmt = "%H:%M"
    for d in dates:
        pt = PrayerTimes(
            coords,
            d,                  # a timezone-aware datetime
            method,             # named method; switch if you want to compare
            time_zone=tz,       # per adhanpy docs, outputs are in this local zone
        )
        print(f"\n{d.strftime('%A %d %B %Y')} (Asia/Karachi)")
        print(f"Fajr:    {pt.fajr.strftime(fmt)}")
        print(f"Sunrise: {pt.sunrise.strftime(fmt)}")
        print(f"Dhuhr:   {pt.dhuhr.strftime(fmt)}")
        print(f"Asr:     {pt.asr.strftime(fmt)}")
        print(f"Maghrib: {pt.maghrib.strftime(fmt)}")
        print(f"Isha:    {pt.isha.strftime(fmt)}")

if __name__ == "__main__":
    # Islamabad
    coords = (33.6844, 73.0479)
    tz = ZoneInfo("Asia/Karachi")

    # Use the same method you tested earlier; you can change to CalculationMethod.KARACHI to compare
    method = CalculationMethod.MOON_SIGHTING_COMMITTEE

    dates = [
        datetime(2025, 8, 28, tzinfo=tz),
        datetime(2025, 8, 29, tzinfo=tz),
        datetime(2025, 8, 30, tzinfo=tz),
        datetime(2025, 8, 31, tzinfo=tz),
    ]

    show_for_dates(dates, coords, method, tz)
