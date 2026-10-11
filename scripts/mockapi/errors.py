from http import HTTPStatus


class ApiError(Exception):
    def __init__(self, status, code, message, details=None, headers=None):
        super().__init__(message)
        self.status = status
        self.code = code
        self.message = message
        self.details = details
        self.headers = headers or {}

    def to_json(self):
        error = {"code": self.code, "message": self.message}
        if self.details:
            error["details"] = self.details
        return {"error": error}


def field_error(field, message):
    return {"field": field, "message": message}


def validation_error(details):
    summary = "; ".join(f"{d['field']}: {d['message']}" for d in details)
    return ApiError(
        HTTPStatus.UNPROCESSABLE_ENTITY, "validation_error", f"Request validation failed: {summary}", details
    )
