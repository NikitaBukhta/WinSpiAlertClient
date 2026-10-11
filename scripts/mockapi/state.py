from dataclasses import dataclass

from .events import EventStore
from .faults import FaultQueue
from .ratelimit import RateLimiter


@dataclass(frozen=True)
class Settings:
    api_key: str
    rate_limit: int
    max_file_size: int


class MockState:
    def __init__(self, settings):
        self.settings = settings
        self.events = EventStore()
        self.faults = FaultQueue()
        self.rate_limiter = RateLimiter(settings.rate_limit)

    def reset(self):
        self.events.clear()
        self.faults.clear()
        self.rate_limiter.reset()
