/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * Minimal mbedTLS configuration for MCUBoot on GD32H759.
 *
 * Only enables the crypto primitives needed for:
 *   - ECDSA-P256 signature verification  (MCUBOOT_SIGN_EC256)
 *   - SHA-256 hashing                   (MCUBOOT_SHA256)
 *
 * Everything else (TLS, X.509, RSA, AES, entropy, etc.) is disabled
 * to minimise code size and RAM usage in the bootloader.
 */

#ifndef MCUBOOT_MBEDTLS_CFG_H
#define MCUBOOT_MBEDTLS_CFG_H

#include <stdint.h>

/* ======================================================================== */
/*  System / platform                                                       */
/* ======================================================================== */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

/* Use standard library malloc/free/realloc by default.
 * The bootloader can override these later if a custom allocator is needed. */

/* ======================================================================== */
/*  Bignum (multi-precision integer) – required by ECC                      */
/* ======================================================================== */
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_BIGNUM_CORE_C
#define MBEDTLS_BIGNUM_MOD_C
#define MBEDTLS_BIGNUM_MOD_RAW_C

/* ======================================================================== */
/*  ASN.1 parsing / writing – required for ECDSA signature & key parsing    */
/* ======================================================================== */
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C

/* ======================================================================== */
/*  OID module – required by PK layer to identify algorithm identifiers     */
/* ======================================================================== */
#define MBEDTLS_OID_C
/* #define MBEDTLS_OID_DUMP */

/* ======================================================================== */
/*  Message digest framework                                                */
/* ======================================================================== */
#define MBEDTLS_MD_C

/* ======================================================================== */
/*  SHA-256                                                                 */
/* ======================================================================== */
#define MBEDTLS_SHA256_C
/* #undef MBEDTLS_SHA512_C */
/* #undef MBEDTLS_SHA1_C   */

/* ======================================================================== */
/*  Elliptic-curve cryptography                                             */
/* ======================================================================== */
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM          /* Use NIST-optimised mod for P256 */

/* ======================================================================== */
/*  ECDSA                                                                   */
/* ======================================================================== */
#define MBEDTLS_ECDSA_C

/* ======================================================================== */
/*  PK (public-key) layer – required by MCUBoot's ECDSA glue code           */
/* ======================================================================== */
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_HAVE_ECC_KEYS

/* ======================================================================== */
/*  Constant-time helpers                                                   */
/* ======================================================================== */
#define MBEDTLS_CONSTANT_TIME_C

/* ======================================================================== */
/*  Error code derivation (optional but helpful for diagnostics)            */
/* ======================================================================== */
/* #define MBEDTLS_ERROR_C */

/* ======================================================================== */
/*  Platform utilities                                                      */
/* ======================================================================== */
#define MBEDTLS_PLATFORM_UTIL_C

/* ======================================================================== */
/*  Features we explicitly do NOT need in the bootloader                    */
/* ======================================================================== */
#undef MBEDTLS_NET_C
#undef MBEDTLS_SSL_CLI_C
#undef MBEDTLS_SSL_SRV_C
#undef MBEDTLS_SSL_TLS_C
#undef MBEDTLS_X509_USE_C
#undef MBEDTLS_X509_CRT_PARSE_C
#undef MBEDTLS_X509_CRL_PARSE_C
#undef MBEDTLS_X509_CSR_PARSE_C
#undef MBEDTLS_X509_CREATE_C
#undef MBEDTLS_X509_WRITE_C
#undef MBEDTLS_X509_WRITE_CRT_C
#undef MBEDTLS_X509_WRITE_CSR_C
#undef MBEDTLS_PKCS5_C
#undef MBEDTLS_PKCS7_C
#undef MBEDTLS_PKCS12_C
#undef MBEDTLS_RSA_C
#undef MBEDTLS_DHM_C
#undef MBEDTLS_ECDH_C
#undef MBEDTLS_ECJPAKE_C
#undef MBEDTLS_AES_C
#undef MBEDTLS_CAMELLIA_C
#undef MBEDTLS_ARIA_C
#undef MBEDTLS_DES_C
#undef MBEDTLS_CHACHA20_C
#undef MBEDTLS_CHACHAPOLY_C
#undef MBEDTLS_GCM_C
#undef MBEDTLS_CCM_C
#undef MBEDTLS_CMAC_C
#undef MBEDTLS_HKDF_C
#undef MBEDTLS_HMAC_DRBG_C
#undef MBEDTLS_CTR_DRBG_C
#undef MBEDTLS_ENTROPY_C
#undef MBEDTLS_PSA_CRYPTO_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_THREADING_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_BASE64_C
#undef MBEDTLS_MD5_C
#undef MBEDTLS_RIPEMD160_C
#undef MBEDTLS_SHA1_C
#undef MBEDTLS_SHA3_C
#undef MBEDTLS_SHA512_C
#undef MBEDTLS_VERSION_C
#undef MBEDTLS_MEMORY_BUFFER_ALLOC_C
#undef MBEDTLS_LMS_C
#undef MBEDTLS_LMOTS_C

/* ======================================================================== */
/*  Mbed TLS v3.x compatibility                                             */
/* ======================================================================== */
/*
 * For Mbed TLS 3.x we must indicate the configuration format version.
 * 0x03000000 = Mbed TLS 3.0+ format (module-level enable macros).
 */
#define MBEDTLS_CONFIG_VERSION  0x03000000

/* ======================================================================== */
/*  PSA compatibility layer – required for Mbed TLS 3.x internals           */
/* ======================================================================== */
/*
 * Even though we don't use PSA Crypto directly, Mbed TLS 3.x internal
 * headers pull in PSA types.  Enabling the minimal compat layer avoids
 * link errors.
 */
/*
 * Do NOT define MBEDTLS_PSA_CRYPTO_CLIENT here.  With mbedTLS v3.x the
 * auto-adjust headers (config_adjust_psa_from_legacy.h etc.) may pull in
 * PSA infrastructure that expects MBEDTLS_PSA_CRYPTO_C to be present when
 * MBEDTLS_PSA_CRYPTO_CLIENT is set.  Since we are building a standalone
 * crypto library (not a PSA client-server split), we do not need the
 * client define.
 */

/* ======================================================================== */
/*  Tweaks for embedded / Cortex-M7                                         */
/* ======================================================================== */
/* #define MBEDTLS_NO_UDBL_DIVISION */

/* 64-bit integer type is available on Cortex-M7 */
/* #define MBEDTLS_NO_64BIT_MULTIPLICATION */

/* Use platform-provided memset/memcpy for zeroisation */
#define MBEDTLS_PLATFORM_ZEROIZE_ALT

/*
 * Memory saving: limit the maximum MPI (bignum) size.
 * P-256 needs at most 256-bit (32-byte) operands.
 * 48 bytes (384 bits) provides headroom for intermediate values.
 */
#define MBEDTLS_MPI_MAX_SIZE            48

/*
 * Reduce ECP window size to save ROM – for verification only
 * (no key generation), a small window is fine.
 */
#define MBEDTLS_ECP_WINDOW_SIZE         2
#define MBEDTLS_ECP_FIXED_POINT_OPTIM   0

#endif /* MCUBOOT_MBEDTLS_CFG_H */