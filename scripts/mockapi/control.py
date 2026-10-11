import json
from http import HTTPStatus

from .faults import Fault, FaultError


def reset(state, data):
    state.reset()
    return HTTPStatus.OK, {"ok": True}


def add_fault(state, data):
    try:
        fault = Fault.from_json(data)
    except FaultError as e:
        return HTTPStatus.BAD_REQUEST, {"error": str(e)}
    state.faults.add(fault)
    return HTTPStatus.OK, fault


def clear_faults(state, data):
    state.faults.clear()
    return HTTPStatus.OK, {"ok": True}


def dump_state(state, data):
    return HTTPStatus.OK, {"events": state.events.all(), "faults": state.faults.list()}


ENDPOINTS = {
    ("POST", "/reset"): reset,
    ("POST", "/faults"): add_fault,
    ("DELETE", "/faults"): clear_faults,
    ("GET", "/state"): dump_state,
}


def handle_control(state, method, path, body):
    try:
        data = json.loads(body) if body else {}
    except ValueError:
        return HTTPStatus.BAD_REQUEST, {"error": "invalid JSON"}

    endpoint = ENDPOINTS.get((method, path))
    if endpoint is None:
        return HTTPStatus.NOT_FOUND, {"error": f"unknown control endpoint {method} {path}"}
    return endpoint(state, data)
