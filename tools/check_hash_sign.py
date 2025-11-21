#!/usr/bin/env python3
# Проверка цепочки хэш -> вторичный хэш -> свертка -> ECDSA (secp192r1)
# Требуется пакет cryptography: python -m pip install cryptography

import argparse
from binascii import unhexlify, hexlify

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature, encode_dss_signature


def fold32_to24(h: bytes) -> bytes:
    """Сворачиваем 32-байтовый хэш в 24 байта XOR'ом верхних 8 байт."""
    if len(h) != 32:
        raise ValueError("Ожидался хэш длиной 32 байта")
    folded = bytearray(h[:24])
    for i in range(8):
        folded[i] ^= h[24 + i]
    return bytes(folded)


def sha256(data: bytes) -> bytes:
    d = hashes.Hash(hashes.SHA256())
    d.update(data)
    return d.finalize()


def build_sign_input(primary_hash: bytes, session_id: bytes) -> tuple[bytes, bytes, bytes]:
    """Возвращает secondary_hash, folded, sign_input (folded+session_id)."""
    secondary = sha256(primary_hash + session_id)
    folded = fold32_to24(secondary)
    sign_input = folded + session_id
    return secondary, folded, sign_input


def sign_raw(priv_hex: str, sign_input: bytes) -> tuple[bytes, bytes, bytes]:
    """Возвращает DER-подпись, raw_r, raw_s (24 байта каждая)."""
    priv_int = int(priv_hex, 16)
    priv_key = ec.derive_private_key(priv_int, ec.SECP192R1())
    der_sig = priv_key.sign(sign_input, ec.ECDSA(hashes.SHA256()))
    r, s = decode_dss_signature(der_sig)
    raw_r = r.to_bytes(24, "big")
    raw_s = s.to_bytes(24, "big")
    return der_sig, raw_r, raw_s


def verify_raw(pub_key, sign_input: bytes, raw_r: bytes, raw_s: bytes) -> bool:
    der = encode_dss_signature(int.from_bytes(raw_r, "big"), int.from_bytes(raw_s, "big"))
    pub_key.verify(der, sign_input, ec.ECDSA(hashes.SHA256()))
    return True


def main():
    parser = argparse.ArgumentParser(description="Проверка вычисления подписи secp192r1 и хэшей")
    parser.add_argument("--primary", help="Первичный хэш (hex, 32 байта)", default="003FE0D7B27539AF77244A7CEA8BB14EAD2E74E1017C686FCCED5A3D33011CC9")
    parser.add_argument("--session", help="Session ID (hex, 24 байта)", default="00"*24)
    parser.add_argument("--priv", help="Приватный ключ (hex, 24 байта)", default="0102030405060708090A0B0C0D0E0F101112131415161718")
    args = parser.parse_args()

    primary_hash = unhexlify(args.primary)
    session_id = unhexlify(args.session)
    priv_hex = args.priv

    secondary, folded, sign_input = build_sign_input(primary_hash, session_id)
    der_sig, raw_r, raw_s = sign_raw(priv_hex, sign_input)

    pub_key = ec.derive_private_key(int(priv_hex, 16), ec.SECP192R1()).public_key()
    verify_raw(pub_key, sign_input, raw_r, raw_s)

    print("Первичный hash     :", hexlify(primary_hash).decode())
    print("Session ID         :", hexlify(session_id).decode())
    print("Вторичный hash     :", hexlify(secondary).decode())
    print("Folded (24 байта)  :", hexlify(folded).decode())
    print("R                  :", hexlify(raw_r).decode())
    print("S                  :", hexlify(raw_s).decode())
    print("DER подпись        :", hexlify(der_sig).decode())
    print("Проверка подписи   : OK")


if __name__ == "__main__":
    main()
