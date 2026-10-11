import datetime
import re

TIMESTAMP_RE = re.compile(r"(?P<seconds>\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})(?:\.(?P<fraction>\d+))?(?:Z|\+00:00)")
MICROSECOND_DIGITS = 6


def iso_now():
    return iso_format(datetime.datetime.now(datetime.timezone.utc))


def iso_format(dt):
    return dt.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def parse_timestamp(value):
    match = TIMESTAMP_RE.fullmatch(value)
    if not match:
        return None

    try:
        dt = datetime.datetime.strptime(match.group("seconds"), "%Y-%m-%dT%H:%M:%S")
    except ValueError:
        return None

    fraction = match.group("fraction") or ""
    microseconds = int(fraction[:MICROSECOND_DIGITS].ljust(MICROSECOND_DIGITS, "0"))
    return dt.replace(microsecond=microseconds, tzinfo=datetime.timezone.utc)
