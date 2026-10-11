import json
import re
from dataclasses import dataclass
from http import HTTPStatus

from .config import API_PREFIX, MAX_JSON_BODY
from .errors import ApiError, field_error, validation_error
from .events import validate_event
from .multipart import parse_multipart
from .timefmt import iso_now

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
JPEG_MAGIC = b"\xff\xd8\xff"

DEFAULT_LIMIT = 20
MAX_LIMIT = 100


@dataclass
class ApiRequest:
    method: str
    full_path: str
    query: dict
    headers: object
    body: bytes | None

    @property
    def path(self):
        return self.full_path[len(API_PREFIX) :]


def handle_api_request(state, request):
    if request.body is None:
        raise ApiError(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "payload_too_large", "Request body is too large")
    if not request.full_path.startswith(API_PREFIX + "/"):
        raise not_found(request)
    check_api_key(request, state.settings.api_key)
    state.rate_limiter.check()
    return route(state, request)


def check_api_key(request, expected_key):
    api_key = request.headers.get("X-API-Key")
    if not api_key:
        raise ApiError(HTTPStatus.UNAUTHORIZED, "missing_api_key", "X-API-Key header is required")
    if api_key != expected_key:
        raise ApiError(HTTPStatus.UNAUTHORIZED, "invalid_api_key", "API key is not valid.")


def get_status(state, request):
    return HTTPStatus.OK, {
        "status": "online",
        "api_version": "1.0",
        "server_time": iso_now(),
        "candidate": "Mock Candidate",
    }


def create_event(state, request):
    require_content_type(request, "application/json")
    require_body(request)
    if len(request.body) > MAX_JSON_BODY:
        raise ApiError(
            HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "payload_too_large", f"Request body exceeds {MAX_JSON_BODY} bytes"
        )

    event, created = state.events.create(validate_event(parse_json(request.body)))
    return (HTTPStatus.CREATED if created else HTTPStatus.OK), event


def upload_image(state, request, event_id):
    event = state.events.get(event_id)
    if not event:
        raise ApiError(HTTPStatus.NOT_FOUND, "event_not_found", f"Event {event_id} not found")
    require_content_type(request, "multipart/form-data")
    require_body(request)

    parts = parse_multipart(request.headers.get("Content-Type", ""), request.body)
    image = extract_image(parts, event.data.file_name)
    check_image(image, event.data.file_name, state.settings.max_file_size)

    state.events.attach_image(event, image)
    return HTTPStatus.OK, event


def list_events(state, request):
    limit = int_query_param(request.query, "limit", DEFAULT_LIMIT, low=1, high=MAX_LIMIT)
    offset = int_query_param(request.query, "offset", 0, low=0)
    image_uploaded = bool_query_param(request.query, "image_uploaded")

    events = state.events.list(image_uploaded)
    return HTTPStatus.OK, {
        "total": len(events),
        "limit": limit,
        "offset": offset,
        "items": events[offset : offset + limit],
    }


@dataclass
class Route:
    method: str
    template: str
    handler: object

    def __post_init__(self):
        self.pattern = re.compile(re.sub(r"\{(\w+)\}", r"(?P<\1>\\d+)", self.template))

    def match_path(self, path):
        match = self.pattern.fullmatch(path)
        return {name: int(value) for name, value in match.groupdict().items()} if match else None


ROUTES = [
    Route("GET", "/status", get_status),
    Route("POST", "/events", create_event),
    Route("GET", "/events", list_events),
    Route("POST", "/events/{event_id}/image", upload_image),
]


def route(state, request):
    path_found = False
    for candidate in ROUTES:
        params = candidate.match_path(request.path)
        if params is None:
            continue
        if candidate.method == request.method:
            return candidate.handler(state, request, **params)
        path_found = True

    if path_found:
        raise ApiError(
            HTTPStatus.METHOD_NOT_ALLOWED,
            "method_not_allowed",
            f"Method {request.method} is not allowed for {request.full_path}",
        )
    raise not_found(request)


def not_found(request):
    return ApiError(HTTPStatus.NOT_FOUND, "not_found", f"Path {request.full_path} not found")


def require_content_type(request, expected):
    if not request.headers.get("Content-Type", "").lower().startswith(expected):
        raise ApiError(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, "unsupported_media_type", f"Content-Type must be {expected}")


def require_body(request):
    if not request.body:
        raise ApiError(HTTPStatus.BAD_REQUEST, "empty_body", "Request body is empty")


def parse_json(body):
    try:
        return json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as e:
        raise ApiError(HTTPStatus.BAD_REQUEST, "invalid_json", f"Invalid JSON: {e}")


def extract_image(parts, registered_file_name):
    file = parts.get("file")
    if file is None or file.file_name is None:
        raise ApiError(
            HTTPStatus.UNPROCESSABLE_ENTITY, "missing_file", "Multipart field 'file' with a filename is required"
        )
    if file.file_name != registered_file_name:
        raise ApiError(
            HTTPStatus.UNPROCESSABLE_ENTITY,
            "file_name_mismatch",
            f"filename '{file.file_name}' does not match registered '{registered_file_name}'",
        )
    return file.data


def check_image(image, file_name, max_file_size):
    if not image:
        raise ApiError(HTTPStatus.UNPROCESSABLE_ENTITY, "empty_file", "Uploaded file is empty")
    if len(image) > max_file_size:
        raise ApiError(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "file_too_large", f"File exceeds {max_file_size} bytes")

    is_png = image.startswith(PNG_MAGIC)
    if not (is_png or image.startswith(JPEG_MAGIC)):
        raise ApiError(HTTPStatus.UNPROCESSABLE_ENTITY, "invalid_image", "File is neither PNG nor JPEG")
    if is_png != file_name.endswith(".png"):
        raise ApiError(
            HTTPStatus.UNPROCESSABLE_ENTITY, "image_format_mismatch", "File content does not match the extension"
        )


def int_query_param(query, name, default, low, high=None):
    raw = query.get(name, [str(default)])[-1]
    try:
        value = int(raw)
    except ValueError:
        value = None

    too_high = value is not None and high is not None and value > high
    if value is None or value < low or too_high:
        limits = f"from {low} to {high}" if high is not None else f">= {low}"
        raise validation_error([field_error(name, f"Input should be an integer {limits}")])
    return value


def bool_query_param(query, name):
    raw = query.get(name, [None])[-1]
    if raw is None:
        return None
    if raw not in ("true", "false"):
        raise validation_error([field_error(name, "Input should be true or false")])
    return raw == "true"
