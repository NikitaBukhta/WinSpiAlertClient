import datetime
import hashlib
import re
import threading
from dataclasses import dataclass
from http import HTTPStatus

from .errors import ApiError, field_error, validation_error
from .timefmt import iso_format, iso_now, parse_timestamp

EVENT_FIELDS = ("event_uid", "event_code", "file_name", "timestamp")

UID_PATTERN = r"^[A-Za-z0-9_-]{8,64}$"
FILE_NAME_PATTERN = r"^[A-Za-z0-9_-]{1,100}\.(png|jpg|jpeg)$"
UID_RE = re.compile(UID_PATTERN)
FILE_NAME_RE = re.compile(FILE_NAME_PATTERN)
MIN_EVENT_CODE = 1
MAX_EVENT_CODE = 10


@dataclass(frozen=True)
class NewEvent:
    uid: str
    code: int
    file_name: str
    timestamp: datetime.datetime


@dataclass
class Event:
    event_id: int
    data: NewEvent
    received_at: str
    image_size: int | None = None
    image_sha256: str | None = None
    image_uploaded_at: str | None = None

    @property
    def image_uploaded(self):
        return self.image_size is not None

    def to_json(self):
        return {
            "event_id": self.event_id,
            "event_uid": self.data.uid,
            "event_code": self.data.code,
            "file_name": self.data.file_name,
            "timestamp": iso_format(self.data.timestamp),
            "received_at": self.received_at,
            "image_uploaded": self.image_uploaded,
            "image_size": self.image_size,
            "image_sha256": self.image_sha256,
            "image_uploaded_at": self.image_uploaded_at,
        }


def check_uid(value):
    if not (isinstance(value, str) and UID_RE.fullmatch(value)):
        return f"String should match pattern '{UID_PATTERN}'"
    return None


def check_code(value):
    if type(value) is not int:  # bool and float are rejected too
        return "Input should be a valid integer"
    if value < MIN_EVENT_CODE:
        return f"Input should be greater than or equal to {MIN_EVENT_CODE}"
    if value > MAX_EVENT_CODE:
        return f"Input should be less than or equal to {MAX_EVENT_CODE}"
    return None


def check_file_name(value):
    if not (isinstance(value, str) and FILE_NAME_RE.fullmatch(value)):
        return f"String should match pattern '{FILE_NAME_PATTERN}'"
    return None


def check_timestamp(value):
    if not (isinstance(value, str) and parse_timestamp(value) is not None):
        return "Input should be an ISO-8601 datetime in UTC"
    return None


FIELD_CHECKS = {
    "event_uid": check_uid,
    "event_code": check_code,
    "file_name": check_file_name,
    "timestamp": check_timestamp,
}


def validate_event(data):
    if not isinstance(data, dict):
        raise validation_error([field_error("body", "Input should be a valid object")])

    details = [field_error(field, "Extra inputs are not permitted") for field in data if field not in EVENT_FIELDS]
    details += [field_error(field, "Field required") for field in EVENT_FIELDS if field not in data]
    for field, check in FIELD_CHECKS.items():
        message = check(data[field]) if field in data else None
        if message:
            details.append(field_error(field, message))

    if details:
        raise validation_error(details)
    return NewEvent(data["event_uid"], data["event_code"], data["file_name"], parse_timestamp(data["timestamp"]))


class EventStore:
    def __init__(self):
        self._lock = threading.Lock()
        self.clear()

    def clear(self):
        with self._lock:
            self._by_id = {}
            self._by_uid = {}
            self._next_id = 1

    def create(self, new_event):
        with self._lock:
            existing = self._by_uid.get(new_event.uid)
            if existing:
                if existing.data != new_event:
                    raise ApiError(
                        HTTPStatus.CONFLICT,
                        "event_uid_conflict",
                        f"event_uid '{new_event.uid}' is already used with different data",
                    )
                return existing, False

            event = Event(self._next_id, new_event, iso_now())
            self._by_id[event.event_id] = event
            self._by_uid[new_event.uid] = event
            self._next_id += 1
            return event, True

    def get(self, event_id):
        with self._lock:
            return self._by_id.get(event_id)

    def attach_image(self, event, image):
        with self._lock:
            if event.image_uploaded:
                raise ApiError(
                    HTTPStatus.CONFLICT,
                    "image_already_uploaded",
                    f"Image for event {event.event_id} is already uploaded",
                )
            event.image_size = len(image)
            event.image_sha256 = hashlib.sha256(image).hexdigest()
            event.image_uploaded_at = iso_now()

    def all(self):
        with self._lock:
            return list(self._by_id.values())

    def list(self, image_uploaded=None):
        with self._lock:
            events = sorted(self._by_id.values(), key=lambda event: event.event_id, reverse=True)
        if image_uploaded is None:
            return events
        return [event for event in events if event.image_uploaded == image_uploaded]
