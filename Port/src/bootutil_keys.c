/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * Public key storage for MCUBoot image signature verification.
 *
 * This file provides the root of trust for MCUBoot's image validation.
 * It contains the public key(s) that MCUBoot uses to verify the
 * ECDSA-P256 signature embedded in each firmware image's TLV area.
 *
 * Key format:
 *   - For MCUBOOT_SIGN_EC256, each key is a raw uncompressed point in
 *     the format: 0x04 || X (32 bytes) || Y (32 bytes), totalling 65 bytes.
 *   - The key is referenced by the image's KEYHASH TLV; MCUBoot computes
 *     a SHA-256 hash of the public key and matches it against the TLV
 *     to select the correct verification key.
 *
 * How to generate a new key pair:
 *
 *   # Generate private key
 *   openssl ecparam -name prime256v1 -genkey -noout -out priv.pem
 *
 *   # Derive public key in uncompressed format
 *   openssl ec -in priv.pem -pubout -out pub.pem
 *
 *   # Convert to raw hex bytes (remove PEM headers and base64-decode,
 *   # or use the imgtool from MCUBoot)
 *   pip install imgtool
 *   imgtool keygen -t ecdsa -k priv.pem
 *   imgtool getpub -k priv.pem -o pub_key.c
 *
 * WARNING:
 *   - The private key (priv.pem) must NEVER be embedded in the bootloader
 *     or shipped with the firmware.
 *   - The public key below should be replaced with your own generated key
 *     before deploying to production hardware.
 *   - Using the default placeholder key in production is a security risk.
 */

#include <bootutil/sign_key.h>
#include <mcuboot_config/mcuboot_config.h>

/*
 * If using hardware key lookup (MCUBOOT_HW_KEY), the key comparison
 * mechanism is different. This file provides the software-based key store.
 */
#if !defined(MCUBOOT_HW_KEY)

/* ========================================================================= */
/*  Public key definition                                                    */
/* ========================================================================= */

/*
 * Default placeholder ECDSA-P256 public key (DO NOT USE IN PRODUCTION).
 *
 * This is a 65-byte uncompressed EC public key:
 *   Byte 0:   0x04 (uncompressed point marker)
 *   Bytes 1-32:  X coordinate (big-endian, 32 bytes)
 *   Bytes 33-64: Y coordinate (big-endian, 32 bytes)
 *
 * Replace this array with your actual public key bytes.
 */
static const unsigned char ec256_pub_key[] = {
    /* Uncompressed point marker */
    0x04,
    /* X coordinate (32 bytes) - REPLACE WITH YOUR KEY */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Y coordinate (32 bytes) - REPLACE WITH YOUR KEY */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* Length of the public key in bytes */
static const unsigned int ec256_pub_key_len = sizeof(ec256_pub_key);

/* ========================================================================= */
/*  Key table (array of key pointers)                                        */
/* ========================================================================= */

/*
 * MCUBoot iterates over this table to find a public key whose SHA-256
 * hash matches the KEYHASH TLV in the image being verified.
 *
 * To support multiple signing keys (e.g. for key rotation), add additional
 * entries to the table and increment the array size accordingly.
 *
 * Example for two keys:
 *   static const unsigned char ec256_pub_key_2[] = { ... };
 *   static const unsigned int ec256_pub_key_len_2 = sizeof(ec256_pub_key_2);
 *
 *   const struct bootutil_key bootutil_keys[2] = {
 *       { .key = ec256_pub_key,     .len = &ec256_pub_key_len     },
 *       { .key = ec256_pub_key_2,   .len = &ec256_pub_key_len_2   },
 *   };
 *   const int bootutil_key_cnt = 2;
 */

const struct bootutil_key bootutil_keys[1] = {
    {
        .key = ec256_pub_key,
        .len = &ec256_pub_key_len,
    },
};

/* Number of entries in the key table */
const int bootutil_key_cnt = 1;

#endif /* !MCUBOOT_HW_KEY */