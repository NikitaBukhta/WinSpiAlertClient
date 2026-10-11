import argparse
import subprocess
import sys
from pathlib import Path

from mockapi import server
from mockapi.config import API_PREFIX, CONTROL_PREFIX, VENV
from mockapi.faults import FAULT_KINDS
from mockapi.state import MockState, Settings

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8443
DEFAULT_API_KEY = "test-key"
DEFAULT_RATE_LIMIT = 60
DEFAULT_MAX_FILE_SIZE = 5 * 1024 * 1024

COMMANDS = {
    "serve": "run the mock server (default)",
    "install-cert": "trust the mock certificate for the current user (CurrentUser\\Root), needed by WinHTTP",
    "uninstall-cert": "remove the mock certificate from CurrentUser\\Root",
    "help": "show this help",
}


def help_epilog():
    commands = "\n".join(f"  {name:<15} {text}" for name, text in COMMANDS.items())
    fault_kinds = "\n                        ".join(f"{kind:<7} {text}" for kind, text in FAULT_KINDS.items())
    return f"""commands:
{commands}

control API (no X-API-Key needed):
  POST   {CONTROL_PREFIX}/reset    clear events, faults and rate limit window
  GET    {CONTROL_PREFIX}/state    dump events and pending faults
  DELETE {CONTROL_PREFIX}/faults   clear pending faults
  POST   {CONTROL_PREFIX}/faults   add a fault, JSON fields:
           fault        {fault_kinds}
           method       HTTP method or "*" (default "*")
           path         glob over the path (default "{API_PREFIX}/*"), e.g. "{API_PREFIX}/events/*/image"
           count        how many matching requests are affected (default 1)

example:
  curl --cacert .mock-server/cert.pem -X POST https://localhost:{DEFAULT_PORT}{CONTROL_PREFIX}/faults ^
       -d "{{\\"fault\\": \\"status\\", \\"status\\": 429, \\"retry_after\\": 2, \\"count\\": 2}}"
"""


def parse_args():
    parser = argparse.ArgumentParser(
        prog="python scripts/mock_server.py",
        description="Local HTTPS mock of the SpiAlert Event API for client tests.",
        epilog=help_epilog(),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("command", nargs="?", default="serve", choices=COMMANDS, metavar="command")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"listen address (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"listen port (default: {DEFAULT_PORT})")
    parser.add_argument("--api-key", default=DEFAULT_API_KEY, help=f"accepted X-API-Key (default: {DEFAULT_API_KEY})")
    parser.add_argument(
        "--rate-limit",
        type=int,
        default=DEFAULT_RATE_LIMIT,
        help=f"requests per minute (default: {DEFAULT_RATE_LIMIT})",
    )
    parser.add_argument(
        "--max-file-size", type=int, default=DEFAULT_MAX_FILE_SIZE, help="max image size in bytes (default: 5 MiB)"
    )
    args = parser.parse_args()
    if args.command == "help":
        parser.print_help()
        sys.exit(0)
    return args


def ensure_venv_python():
    if Path(sys.prefix).resolve() == VENV.resolve():
        return
    from project import BIN, ensure_venv

    ensure_venv()
    try:
        exit_code = subprocess.run(
            [str(BIN / "python"), str(Path(__file__).resolve()), *sys.argv[1:]], check=False
        ).returncode
    except KeyboardInterrupt:
        exit_code = 0
    sys.exit(exit_code)


def main():
    args = parse_args()
    ensure_venv_python()

    from mockapi import certificate  # needs 'cryptography', available only after ensureVenvPython()

    if args.command == "install-cert":
        sys.exit(certificate.install())
    if args.command == "uninstall-cert":
        sys.exit(certificate.uninstall())

    certificate.ensure_certificate()
    settings = Settings(api_key=args.api_key, rate_limit=args.rate_limit, max_file_size=args.max_file_size)
    server.serve(MockState(settings), args.host, args.port)


if __name__ == "__main__":
    main()
