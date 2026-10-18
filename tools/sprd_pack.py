#!/usr/bin/env python3
"""Wrap a U-Boot binary into a Spreadtrum DHTB partition image (1 MiB).

Layout produced (matches every SL8541E/SC9832E watch firmware dump seen so far):

    0x000 .. 0x200      DHTB header, 512 bytes, only [0x30] is meaningful
    0x200 .. 0x200+n    U-Boot payload (u-boot-dtb.bin), 16-byte padded
    0x200+n .. +0x2B4   SEC block: sechdr (0x60) + cert (0x254)
    .. 0x100000         zero fill

The header and the SEC block are copied verbatim from a donor image (the stock
partition dump), because the SEC block holds the RSA key material that the SPL
and TOS expect to find at a fixed offset.  Only the payload length fields are
rewritten:

    header[0x30] = payload length
    sechdr[0x10] = payload length
    sechdr[0x28] = absolute file offset of the cert block

Note: the certificate itself is signed over the *stock* payload, so it no
longer matches once the payload is replaced.  That is fine on a device whose
TOS has AVB removed (see the unlock tools), and it is why this script keeps
the block instead of dropping it: the offsets must still be sane.

Usage:
    python3 tools/sprd_pack.py donor-uboot.img out/u-boot-dtb.bin uboot-new.img
"""
import argparse
import hashlib
import os
import struct
import sys

PART_SIZE = 0x100000
HDR_SIZE = 0x200
DHTB_MAGIC = b"DHTB"
SEC_HDR_SIZE = 0x60
CERT_SIZE = 0x254
SEC_SIZE = SEC_HDR_SIZE + CERT_SIZE
ALIGN = 16


def load_sec(blob):
    """Return (header, payload, sec_block) taken from a donor image."""
    if len(blob) < PART_SIZE:
        sys.exit("donor image smaller than 1 MiB, is it a partition dump?")
    if blob[0:4] != DHTB_MAGIC:
        sys.exit("donor image has no DHTB magic")
    body = struct.unpack_from("<I", blob, 0x30)[0]
    sec_off = HDR_SIZE + body
    sec = blob[sec_off:sec_off + SEC_SIZE]
    if len(sec) != SEC_SIZE:
        sys.exit("donor SEC block truncated (body size 0x%x looks wrong)" % body)
    return blob[0:HDR_SIZE], body, sec


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("donor", help="stock DHTB partition image (1 MiB)")
    ap.add_argument("payload", help="u-boot-dtb.bin produced by the build")
    ap.add_argument("out", help="output image")
    ap.add_argument("--align", type=int, default=ALIGN,
                    help="payload alignment, default 16 (vendor uses 16)")
    args = ap.parse_args()

    with open(args.donor, "rb") as f:
        donor = f.read()
    with open(args.payload, "rb") as f:
        payload = f.read()
    if not payload:
        sys.exit("empty payload")

    hdr, donor_body, sec = load_sec(donor)

    body = len(payload)
    if args.align > 1:
        body = (body + args.align - 1) // args.align * args.align
    end = HDR_SIZE + body + SEC_SIZE
    if end > PART_SIZE:
        sys.exit("payload too big: 0x%x bytes does not fit in 1 MiB" % len(payload))

    img = bytearray(PART_SIZE)
    img[0:HDR_SIZE] = hdr
    struct.pack_into("<I", img, 0x30, body)
    img[HDR_SIZE:HDR_SIZE + len(payload)] = payload

    sec = bytearray(sec)
    struct.pack_into("<Q", sec, 0x10, body)
    struct.pack_into("<Q", sec, 0x28, HDR_SIZE + body + SEC_HDR_SIZE)
    img[HDR_SIZE + body:HDR_SIZE + body + SEC_SIZE] = sec

    with open(args.out, "wb") as f:
        f.write(img)

    print("donor body    : 0x%x" % donor_body)
    print("payload       : %d -> 0x%x (%d%%)" % (len(payload), body, 100 * body // PART_SIZE))
    print("header[0x30]  : 0x%x" % body)
    print("sec block     : 0x%x .. 0x%x" % (end - SEC_SIZE, end))
    print("output        : %s  %d bytes  md5 %s" %
          (args.out, PART_SIZE, hashlib.md5(img).hexdigest()))


if __name__ == "__main__":
    main()
