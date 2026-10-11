import datetime
import ipaddress
import subprocess

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID

from .config import CERT_CER, CERT_DIR, CERT_PEM, KEY_PEM

COMMON_NAME = "WinSpiAlertClient mock server (localhost)"
VALIDITY_DAYS = 365


def ensure_certificate():
    if CERT_PEM.exists() and KEY_PEM.exists():
        if load_certificate().not_valid_after_utc > utc_now():
            return
        print("Certificate expired, regenerating. Run 'uninstall-cert' and 'install-cert' again.")

    key = ec.generate_private_key(ec.SECP256R1())
    certificate = build_certificate(key)

    CERT_DIR.mkdir(exist_ok=True)
    KEY_PEM.write_bytes(
        key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    )
    CERT_PEM.write_bytes(certificate.public_bytes(serialization.Encoding.PEM))
    CERT_CER.write_bytes(certificate.public_bytes(serialization.Encoding.DER))
    print(f"Generated certificate: {CERT_PEM}")


def build_certificate(key):
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, COMMON_NAME)])
    hosts = x509.SubjectAlternativeName([x509.DNSName("localhost"), x509.IPAddress(ipaddress.ip_address("127.0.0.1"))])
    now = utc_now()
    return (
        x509.CertificateBuilder()
        .subject_name(name)
        .issuer_name(name)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1))
        .not_valid_after(now + datetime.timedelta(days=VALIDITY_DAYS))
        .add_extension(hosts, critical=False)
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
        .add_extension(server_key_usage(), critical=True)
        .sign(key, hashes.SHA256())
    )


def server_key_usage():
    return x509.KeyUsage(
        digital_signature=True,
        content_commitment=False,
        key_encipherment=False,
        data_encipherment=False,
        key_agreement=False,
        key_cert_sign=False,
        crl_sign=False,
        encipher_only=False,
        decipher_only=False,
    )


def install():
    ensure_certificate()
    print("Adding the mock certificate to CurrentUser\\Root. Windows will ask for confirmation.")
    return subprocess.run(["certutil", "-user", "-addstore", "Root", str(CERT_CER)], check=False).returncode


def uninstall():
    if not CERT_PEM.exists():
        print("No certificate generated, nothing to remove")
        return 1
    return subprocess.run(["certutil", "-user", "-delstore", "Root", thumbprint()], check=False).returncode


def thumbprint():
    return load_certificate().fingerprint(hashes.SHA1()).hex()


def load_certificate():
    return x509.load_pem_x509_certificate(CERT_PEM.read_bytes())


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc)
