import re
from dataclasses import dataclass
from http import HTTPStatus

from .errors import ApiError

BOUNDARY_RE = re.compile(r'boundary="?([^";]+)"?')
NAME_RE = re.compile(r'\bname="([^"]*)"')
FILE_NAME_RE = re.compile(r'\bfilename="([^"]*)"')
CRLF = b"\r\n"


@dataclass
class Part:
    file_name: str | None
    content_type: str | None
    data: bytes


def invalid_multipart(message):
    return ApiError(HTTPStatus.BAD_REQUEST, "invalid_multipart", message)


def parse_multipart(content_type, body):
    boundary = parse_boundary(content_type)
    parts = {}
    for raw_part in split_parts(body, boundary):
        name, part = parse_part(raw_part)
        parts[name] = part
    return parts


def parse_boundary(content_type):
    match = BOUNDARY_RE.search(content_type)
    if not match:
        raise invalid_multipart("Missing multipart boundary")
    return match.group(1).encode()


def split_parts(body, boundary):
    opening = b"--" + boundary + CRLF
    separator = CRLF + b"--" + boundary + CRLF
    closing = CRLF + b"--" + boundary + b"--"
    if not body.startswith(opening) or closing not in body:
        raise invalid_multipart("Malformed multipart body")
    return body[len(opening) : body.index(closing)].split(separator)


def parse_part(raw_part):
    header_block, separator, data = raw_part.partition(CRLF + CRLF)
    if not separator:
        raise invalid_multipart("Malformed multipart part headers")

    headers = parse_part_headers(header_block)
    disposition = headers.get("content-disposition", "")
    name = NAME_RE.search(disposition)
    if not disposition.startswith("form-data") or not name:
        raise invalid_multipart("Part without form-data Content-Disposition name")

    file_name = FILE_NAME_RE.search(disposition)
    return name.group(1), Part(file_name.group(1) if file_name else None, headers.get("content-type"), data)


def parse_part_headers(header_block):
    headers = {}
    for line in header_block.decode("utf-8", "replace").split("\r\n"):
        key, _, value = line.partition(":")
        headers[key.strip().lower()] = value.strip()
    return headers
