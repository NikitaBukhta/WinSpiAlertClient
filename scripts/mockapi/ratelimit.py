import math
import threading
import time
from collections import deque
from http import HTTPStatus

from .config import RATE_WINDOW_SEC
from .errors import ApiError


class RateLimiter:
    def __init__(self, requests_per_window):
        self.requests_per_window = requests_per_window
        self._lock = threading.Lock()
        self._request_times = deque()

    def reset(self):
        with self._lock:
            self._request_times.clear()

    def check(self):
        with self._lock:
            now = time.monotonic()
            self._drop_expired(now)
            if len(self._request_times) >= self.requests_per_window:
                raise ApiError(
                    HTTPStatus.TOO_MANY_REQUESTS,
                    "rate_limited",
                    f"Rate limit exceeded: {self.requests_per_window} requests per minute",
                    headers={"Retry-After": str(self._seconds_until_free(now))},
                )
            self._request_times.append(now)

    def _drop_expired(self, now):
        while self._request_times and now - self._request_times[0] >= RATE_WINDOW_SEC:
            self._request_times.popleft()

    def _seconds_until_free(self, now):
        oldest_expires_at = self._request_times[0] + RATE_WINDOW_SEC
        return max(1, math.ceil(oldest_expires_at - now))
