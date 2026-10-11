import json
import ssl
import sys
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

from .config import API_PREFIX, CERT_PEM, CONTROL_PREFIX, KEY_PEM, MULTIPART_OVERHEAD
from .control import handle_control
from .endpoints import ApiRequest, handle_api_request
from .errors import ApiError

HTML_ERROR_BODY = b"<html><body><h1>502 Bad Gateway</h1></body></html>"
TRUNCATED_BODY = b'{"status": "online", "api_version": "1.0", "candidate": "Mock Candidate"}'
FAULT_ERROR_CODES = {429: "rate_limited", 500: "internal_error"}
DEFAULT_FAULT_ERROR_CODE = "injected_error"
DEFAULT_FAULT_MESSAGE = "Injected by mock server"


def to_json(value):
    return value.to_json()


class RequestHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "SpiAlertMock/1.0"
    state = None

    def do_GET(self):
        self.dispatch()

    do_POST = do_PUT = do_DELETE = do_PATCH = do_GET

    def log_message(self, format, *args):
        pass

    def dispatch(self):
        url = urlsplit(self.path)
        body = self.read_body()
        if url.path.startswith(CONTROL_PREFIX):
            status, payload = handle_control(self.state, self.command, url.path[len(CONTROL_PREFIX) :], body or b"")
            self.send_json(status, payload)
            return

        try:
            self.handle_api(url, body)
        except Exception as e:  # noqa: BLE001
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, internal_error(f"Mock server error: {e}").to_json())
            self.log_result(HTTPStatus.INTERNAL_SERVER_ERROR, [f"internal_error: {e!r}"])

    def handle_api(self, url, body):
        notes = [] if self.headers.get("User-Agent") else ["WARNING: no User-Agent"]

        fault = self.state.faults.take(self.command, url.path)
        if fault:
            notes.append(f"fault: {fault.kind}")
            if fault.kind == "delay":
                time.sleep(fault.delay_seconds)
            elif fault.kind != "drop":
                self.respond_with_fault(fault, notes)
                return

        request = ApiRequest(self.command, url.path, parse_qs(url.query, keep_blank_values=True), self.headers, body)
        try:
            status, payload = handle_api_request(self.state, request)
            headers = {}
        except ApiError as e:
            status, payload, headers = e.status, e.to_json(), e.headers
            notes.append(e.code)

        if fault and fault.kind == "drop":
            self.close_connection = True
            self.log_result("dropped", notes + [f"processed as {int(status)}"])
            return

        self.send_json(status, payload, headers)
        self.log_result(status, notes)

    def respond_with_fault(self, fault, notes):
        if fault.kind == "reset":
            self.close_connection = True
            self.log_result("reset", notes)
        elif fault.kind == "html":
            self.send_raw(HTTPStatus.BAD_GATEWAY, HTML_ERROR_BODY, "text/html")
            self.log_result(HTTPStatus.BAD_GATEWAY, notes)
        elif fault.kind == "status":
            status = fault.response_status
            code = fault.code if fault.code is not None else FAULT_ERROR_CODES.get(status, DEFAULT_FAULT_ERROR_CODE)
            message = fault.message if fault.message is not None else DEFAULT_FAULT_MESSAGE
            error = ApiError(status, code, message)
            headers = {"Retry-After": str(fault.retry_after)} if fault.retry_after is not None else {}
            self.send_json(status, error.to_json(), headers)
            self.log_result(status, notes)
        elif fault.kind == "truncate":
            self.send_truncated(TRUNCATED_BODY)
            self.log_result("truncated", notes)

    def send_truncated(self, body):
        self.close_connection = True
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body[: len(body) // 2])

    def read_body(self):
        length = int(self.headers.get("Content-Length") or 0)
        if length > self.state.settings.max_file_size + MULTIPART_OVERHEAD:
            self.close_connection = True  # the body is not consumed, so the connection cannot be reused
            return None
        return self.rfile.read(length) if length > 0 else b""

    def send_json(self, status, payload, headers=None):
        body = json.dumps(payload, ensure_ascii=False, default=to_json).encode("utf-8")
        self.send_raw(status, body, "application/json", headers)

    def send_raw(self, status, body, content_type, headers=None):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        for key, value in (headers or {}).items():
            self.send_header(key, value)
        if self.close_connection:
            self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def log_result(self, outcome, notes):
        outcome = int(outcome) if isinstance(outcome, int) else outcome
        details = f"  ({'; '.join(notes)})" if notes else ""
        print(f"[{time.strftime('%H:%M:%S')}] {self.command} {self.path} -> {outcome}{details}", flush=True)


def internal_error(message):
    return ApiError(HTTPStatus.INTERNAL_SERVER_ERROR, "internal_error", message)


class MockHttpServer(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, request, client_address):
        # WinHTTP resets a keep-alive connection when its handle is closed: not a server error
        if isinstance(sys.exc_info()[1], ConnectionError):
            return
        super().handle_error(request, client_address)


def serve(state, host, port):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(CERT_PEM, KEY_PEM)

    RequestHandler.state = state
    server = MockHttpServer((host, port), RequestHandler)
    server.socket = context.wrap_socket(server.socket, server_side=True)

    print(f"Mock Event API: https://localhost:{port}{API_PREFIX}  (X-API-Key: {state.settings.api_key})")
    print(f"Control API:    https://localhost:{port}{CONTROL_PREFIX}/[reset|faults|state]")
    print("Ctrl+C to stop", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
