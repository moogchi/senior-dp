#!/usr/bin/env python3

import hashlib
import hmac

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import (
    Encoding,
    PrivateFormat,
    PublicFormat,
    NoEncryption,
)


SHA256_LEN = 32


def hkdf_extract(salt: bytes, ikm: bytes) -> bytes:
    """
    HKDF-Extract using HMAC-SHA256.
    """
    return hmac.new(salt, ikm, hashlib.sha256).digest()


def hkdf_expand(prk: bytes, info: bytes, length: int) -> bytes:
    """
    HKDF-Expand using HMAC-SHA256.
    RFC 5869.
    """
    if length > 255 * SHA256_LEN:
        raise ValueError("Requested HKDF output is too large")

    okm = b""
    t = b""
    counter = 1

    while len(okm) < length:
        t = hmac.new(
            prk,
            t + info + bytes([counter]),
            hashlib.sha256,
        ).digest()

        okm += t
        counter += 1

    return okm[:length]


def ed25519_key_pair(seed: bytes):
    """
    Generate Ed25519 key pair from a 32-byte seed.
    """
    if len(seed) != 32:
        raise ValueError("Ed25519 seed must be 32 bytes")

    private_key = Ed25519PrivateKey.from_private_bytes(seed)

    secret_key = private_key.private_bytes(
        encoding=Encoding.Raw,
        format=PrivateFormat.Raw,
        encryption_algorithm=NoEncryption(),
    )

    public_key = private_key.public_key().public_bytes(
        encoding=Encoding.Raw,
        format=PublicFormat.Raw,
    )

    return secret_key, public_key


def print_hex(name: str, data: bytes):
    print(f"{name}: {data.hex()}")


def main():

    # ------------------------------------------------------------
    # EXACTLY MATCHES CURRENT C TEST
    # ------------------------------------------------------------

    # Your C array specifies 22 bytes of 0x0b.
    # Since the array size is 32, C automatically initializes
    # the remaining 10 bytes to zero.
    uds = bytes([
        0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b, 0x0b, 0x0b,

        0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b, 0x0b, 0x0b,

        0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b,

        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    ])

    # uint8_t salt[32] = {0};
    salt = bytes(32)

    # uint8_t info[32] = {0};
    info = bytes(32)

    # ------------------------------------------------------------
    # HKDF EXTRACT
    # ------------------------------------------------------------

    prk = hkdf_extract(
        salt=salt,
        ikm=uds,
    )

    # ------------------------------------------------------------
    # HKDF EXPAND
    # ------------------------------------------------------------

    cdi = hkdf_expand(
        prk=prk,
        info=info,
        length=32,
    )

    # ------------------------------------------------------------
    # ED25519
    # ------------------------------------------------------------

    secret_key, public_key = ed25519_key_pair(cdi)

    # ------------------------------------------------------------
    # TEST VECTOR
    # ------------------------------------------------------------

    print("========== INPUTS ==========")

    print_hex("uds", uds)
    print_hex("salt", salt)
    print_hex("info", info)

    print()
    print("========== HKDF ==========")

    print_hex("prk", prk)
    print_hex("cdi", cdi)

    print()
    print("========== ED25519 ==========")

    print_hex("public_key", public_key)
    print_hex("private_key", secret_key)


if __name__ == "__main__":
    main()