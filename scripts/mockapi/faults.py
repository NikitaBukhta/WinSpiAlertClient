import fnmatch
import threading
from dataclasses import dataclass

from .config import API_PREFIX

DEFAULT_FAULT_STATUS = 500
DEFAULT_DELAY_SEC = 5

FAULT_KINDS = {
    "drop": "process the request, then close the connection without a response",
    "reset": "close the connection without processing",
    "status": f'respond with "status" (default {DEFAULT_FAULT_STATUS}), optional "retry_after", "code", "message"',
    "html": "respond 502 with an HTML body",
    "delay": f'sleep "seconds" (default {DEFAULT_DELAY_SEC}), then process normally',
}

OPTIONAL_FIELDS = {
    "method": str,
    "path": str,
    "count": int,
    "status": int,
    "retry_after": int,
    "code": str,
    "message": str,
    "seconds": (int, float),
}


class FaultError(ValueError):
    pass


@dataclass
class Fault:
    kind: str
    method: str = "*"
    path: str = API_PREFIX + "/*"
    count: int = 1
    status: int | None = None
    retry_after: int | None = None
    code: str | None = None
    message: str | None = None
    seconds: float | None = None

    @staticmethod
    def from_json(data):
        if not isinstance(data, dict) or data.get("fault") not in FAULT_KINDS:
            raise FaultError(f"fault must be one of {', '.join(FAULT_KINDS)}")

        fields = {key: value for key, value in data.items() if key != "fault"}
        for key, value in fields.items():
            if key not in OPTIONAL_FIELDS:
                raise FaultError(f"unknown field '{key}'")
            if not isinstance(value, OPTIONAL_FIELDS[key]):
                raise FaultError(f"field '{key}' has a wrong type")
        return Fault(kind=data["fault"], **fields)

    def to_json(self):
        json = {"fault": self.kind, "method": self.method, "path": self.path, "count": self.count}
        optional = {key: getattr(self, key) for key in ("status", "retry_after", "code", "message", "seconds")}
        json.update({key: value for key, value in optional.items() if value is not None})
        return json

    def matches(self, method, path):
        return self.method in ("*", method) and fnmatch.fnmatchcase(path, self.path)

    @property
    def response_status(self):
        return self.status if self.status is not None else DEFAULT_FAULT_STATUS

    @property
    def delay_seconds(self):
        return self.seconds if self.seconds is not None else DEFAULT_DELAY_SEC


class FaultQueue:
    def __init__(self):
        self._lock = threading.Lock()
        self._faults = []

    def add(self, fault):
        with self._lock:
            self._faults.append(fault)

    def clear(self):
        with self._lock:
            self._faults.clear()

    def list(self):
        with self._lock:
            return list(self._faults)

    def take(self, method, path):
        with self._lock:
            for fault in self._faults:
                if fault.matches(method, path):
                    fault.count -= 1
                    if fault.count <= 0:
                        self._faults.remove(fault)
                    return fault
            return None
