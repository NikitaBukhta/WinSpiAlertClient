from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VENV = ROOT / ".venv"

CERT_DIR = ROOT / ".mock-server"
CERT_PEM = CERT_DIR / "cert.pem"
KEY_PEM = CERT_DIR / "key.pem"
CERT_CER = CERT_DIR / "cert.cer"

API_PREFIX = "/api/v1"
CONTROL_PREFIX = "/__mock__"

MAX_JSON_BODY = 64 * 1024
MULTIPART_OVERHEAD = 64 * 1024
RATE_WINDOW_SEC = 60
