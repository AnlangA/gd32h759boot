"""Generate ECDSA-P256 key pair and inject public key into bootutil_keys.c."""

import sys
import re
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization


def generate_keypair(pem_path: Path):
    private_key = ec.generate_private_key(ec.SECP256R1())
    pem_path.parent.mkdir(parents=True, exist_ok=True)
    pem_path.write_bytes(
        private_key.private_bytes(
            serialization.Encoding.PEM,
            serialization.PrivateFormat.PKCS8,
            serialization.NoEncryption(),
        )
    )
    return private_key


def raw_pubkey_bytes(private_key) -> bytes:
    """Extract 65-byte uncompressed point: 0x04 || X (32B) || Y (32B)."""
    pub = private_key.public_key()
    return pub.public_bytes(
        serialization.Encoding.X962,
        serialization.PublicFormat.UncompressedPoint,
    )


def hex_lines(data: bytes, per_line: int = 8) -> list[str]:
    hex_vals = [f"0x{b:02x}" for b in data]
    return [
        ", ".join(hex_vals[i : i + per_line]) + ","
        for i in range(0, len(hex_vals), per_line)
    ]


def inject_pubkey(keys_c_path: Path, pubkey: bytes):
    src = keys_c_path.read_text(encoding="utf-8")

    # Match the whole array body including comments between { and };
    pattern = (
        r"(static\s+const\s+unsigned\s+char\s+ec256_pub_key\[\]\s*=\s*\{)"
        r".*?"
        r"(\};)"
    )
    match = re.search(pattern, src, re.DOTALL)
    if not match:
        print("ERROR: could not find ec256_pub_key array in", keys_c_path)
        sys.exit(1)

    lines = hex_lines(pubkey)
    x_lines = lines[:4]
    y_lines = lines[4:]

    body = (
        "\n"
        "    /* Uncompressed point marker */\n"
        f"    {lines[0]}\n"
        "    /* X coordinate (32 bytes) */\n"
        + "\n".join(f"    {l}" for l in x_lines[1:])
        + "\n"
        "    /* Y coordinate (32 bytes) */\n"
        + "\n".join(f"    {l}" for l in y_lines)
        + "\n"
    )

    new_src = src[: match.start()] + match.group(1) + body + match.group(2) + src[match.end() :]
    keys_c_path.write_text(new_src, encoding="utf-8")


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_PEM = SCRIPT_DIR / "keys" / "ecdsa256.pem"
DEFAULT_KEYS_C = SCRIPT_DIR.parent / "Port" / "src" / "bootutil_keys.c"


def main():
    pem_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_PEM
    keys_c_path = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_KEYS_C

    private_key = generate_keypair(pem_path)
    pubkey = raw_pubkey_bytes(private_key)
    inject_pubkey(keys_c_path, pubkey)

    print(f"Private key: {pem_path}")
    print(f"Public key injected into: {keys_c_path}")
    print(f"Public key ({len(pubkey)} bytes): {pubkey.hex()}")


if __name__ == "__main__":
    main()
