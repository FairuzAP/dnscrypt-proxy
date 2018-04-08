
#include <config.h>
#include <sys/types.h>

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sodium.h>
#include "salsa20_random.h"
#include "safe_rw.h"
#include "utils.h"

#ifdef _WIN32
# include <Windows.h>
# include <Wincrypt.h>
#endif

#define SALSA20_RANDOM_BLOCK_SIZE crypto_core_salsa20_OUTPUTBYTES
#define SHA256_BLOCK_SIZE 64U
#define SHA256_MIN_PAD_SIZE (1U + 8U)

typedef struct Salsa20Random_ {
    unsigned char key[crypto_stream_salsa20_KEYBYTES];
    unsigned char rnd32[SALSA20_RANDOM_BLOCK_SIZE];
    uint64_t      nonce;
    size_t        rnd32_outleft;
    pid_t         pid;
#ifdef _WIN32
    HCRYPTPROV    hcrypt_prov;
#endif
    int           random_data_source_fd;
    _Bool         initialized;
} Salsa20Random;

static Salsa20Random stream = {
    .random_data_source_fd = -1,
    .rnd32_outleft = (size_t) 0U,
    .initialized = 0
};

#ifndef _WIN32
static int
salsa20_random_random_dev_open(void)
{
    static const char * const devices[] = {
# ifndef USE_BLOCKING_RANDOM
        "/dev/arandom", "/dev/urandom",
# endif
        "/dev/random", NULL
    };
    const char * const *device = devices;

    do {
        if (access(*device, F_OK | R_OK) == 0) {
            return open(*device, O_RDONLY);
        }
        device++;
    } while (*device != NULL);

    return -1;
}

static void
salsa20_random_init(void)
{
    stream.nonce = dnscrypt_hrtime();
    assert(stream.nonce != (uint64_t) 0U);

    if ((stream.random_data_source_fd =
         salsa20_random_random_dev_open()) == -1) {
        abort();
    }
}

#else /* _WIN32 */

static void
salsa20_random_init(void)
{
    stream.nonce = dnscrypt_hrtime();
    assert(stream.nonce != (uint64_t) 0U);

    if (! CryptAcquireContext(&stream.hcrypt_prov, NULL, NULL,
                              PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        abort();
    }
}
#endif

void
salsa20_random_stir(void)
{
    unsigned char  m0[3U * SHA256_BLOCK_SIZE - SHA256_MIN_PAD_SIZE];
    unsigned char  m1[SHA256_BLOCK_SIZE + crypto_hash_sha256_BYTES];
    unsigned char *k0 = m0 + SHA256_BLOCK_SIZE;
    unsigned char *k1 = m1 + SHA256_BLOCK_SIZE;
    size_t         i;
    size_t         sizeof_k0 = sizeof m0 - SHA256_BLOCK_SIZE;

    memset(stream.rnd32, 0, sizeof stream.rnd32);
    stream.rnd32_outleft = (size_t) 0U;
    if (stream.initialized == 0) {
        salsa20_random_init();
        stream.initialized = 1;
    }
    memset(m0, 0x69, SHA256_BLOCK_SIZE);
    memset(m1, 0x42, SHA256_BLOCK_SIZE);
#ifndef _WIN32
    if (safe_read(stream.random_data_source_fd, k0,
                  sizeof_k0) != (ssize_t) sizeof_k0) {
        abort();
    }
#else /* _WIN32 */
    if (! CryptGenRandom(stream.hcrypt_prov, sizeof_k0, k0)) {
        abort();
    }
#endif
    COMPILER_ASSERT(sizeof m0 >= 2U * SHA256_BLOCK_SIZE);
    crypto_hash_sha256(k1, m0, sizeof m0);
    COMPILER_ASSERT(sizeof m1 >= SHA256_BLOCK_SIZE + crypto_hash_sha256_BYTES);
    crypto_hash_sha256(stream.key, m1, sizeof m1);
    dnscrypt_memzero(m1, sizeof m1);
    COMPILER_ASSERT(sizeof stream.key == crypto_hash_sha256_BYTES);
    assert(sizeof stream.key <= sizeof_k0);
    for (i = (size_t) 0U; i < sizeof stream.key; i++) {
        stream.key[i] ^= k0[i];
    }
    dnscrypt_memzero(m0, sizeof m0);
}

static void
salsa20_random_stir_if_needed(void)
{
    const pid_t pid = getpid();

    if (stream.initialized == 0 || stream.pid != pid) {
        stream.pid = pid;
        salsa20_random_stir();
    }
}

static uint32_t
salsa20_random_getword(void)
{
    uint32_t val;

    COMPILER_ASSERT(sizeof stream.rnd32 >= sizeof val);
    COMPILER_ASSERT(sizeof stream.rnd32 % sizeof val == (size_t) 0U);
    if (stream.rnd32_outleft <= (size_t) 0U) {
        COMPILER_ASSERT(sizeof stream.nonce == crypto_stream_salsa20_NONCEBYTES);
        assert(crypto_stream_salsa20((unsigned char *) stream.rnd32,
                                     (unsigned long long) sizeof stream.rnd32,
                                     (unsigned char *) &stream.nonce,
                                     stream.key) == 0);
        stream.nonce++;
        stream.rnd32_outleft = sizeof stream.rnd32;
    }
    stream.rnd32_outleft -= sizeof val;
    memcpy(&val, &stream.rnd32[stream.rnd32_outleft], sizeof val);

    return val;
}

int
salsa20_random_close(void)
{
    int ret = -1;

#ifndef _WIN32
    if (stream.random_data_source_fd != -1 &&
        close(stream.random_data_source_fd) == 0) {
        stream.random_data_source_fd = -1;
        stream.initialized = 0;
        ret = 0;
    }
#else /* _WIN32 */
    if (stream.initialized != 0 &&
        CryptReleaseContext(stream.hcrypt_prov, 0)) {
        stream.initialized = 0;
        ret = 0;
    }
#endif
    return ret;
}

uint32_t
salsa20_random(void)
{
    salsa20_random_stir_if_needed();

    return salsa20_random_getword();
}

void
salsa20_random_buf(void * const buf, const size_t size)
{
    salsa20_random_stir_if_needed();
    COMPILER_ASSERT(sizeof stream.nonce == crypto_stream_salsa20_NONCEBYTES);
#ifdef ULONG_LONG_MAX
    assert(size <= ULONG_LONG_MAX);
#endif
    assert(crypto_stream_salsa20(buf, (unsigned long long) size,
                                 (unsigned char *) &stream.nonce,
                                 stream.key) == 0);
    stream.nonce++;
}

/*
 * salsa20_random_uniform() derives from OpenBSD's arc4random_uniform()
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 */

uint32_t
salsa20_random_uniform(const uint32_t upper_bound)
{
    uint32_t min;
    uint32_t r;

    if (upper_bound < 2) {
        return 0;
    }
    min = (uint32_t) (-upper_bound % upper_bound);
    for (;;) {
        r = salsa20_random();
        if (r >= min) {
            break;
        }
    }
    return r % upper_bound;
}
