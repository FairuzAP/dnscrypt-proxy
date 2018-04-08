#ifndef DNSCRYPT_CUDA
#define DNSCRYPT_CUDA

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <openssl/aes.h>
#include <sodium.h>

#define MSG_MAX 1000000
#define GETU32(pt) (((u32)(pt)[0] << 24) ^ ((u32)(pt)[1] << 16) ^ ((u32)(pt)[2] <<  8) ^ ((u32)(pt)[3]))
#define PUTU32(ct, st) { (ct)[0] = (u8)((st) >> 24); (ct)[1] = (u8)((st) >> 16); (ct)[2] = (u8)((st) >>  8); (ct)[3] = (u8)(st); }
typedef unsigned int u32;
typedef unsigned char u8;

int
crypto_box_afternm_ref(unsigned char *c, const unsigned char *m,
                       unsigned long long mlen,
                       const unsigned char *n,
                       const unsigned char *k);
int
crypto_box_open_afternm_ref(unsigned char *m, const unsigned char *c,
                            unsigned long long clen,
                            const unsigned char *n,
                            const unsigned char *k);
int 
aespoly1305_afternm_ref(unsigned char *c,
						const unsigned char *m,unsigned long long mlen,
						const unsigned char *n,
						const unsigned char *k);
int 
aespoly1305_open_afternm_ref(unsigned char *m,
							 const unsigned char *c,unsigned long long clen,
							 const unsigned char *n,
							 const unsigned char *k);

void
init_cuda();

void
set_aes_key(const unsigned char *key);

int
crypto_box_afternm_cuda(unsigned char *c, const unsigned char *m,
                        unsigned long long mlen,
                        const unsigned char *n,
                        const unsigned char *k);
int
crypto_box_open_afternm_cuda(unsigned char *m, const unsigned char *c,
                             unsigned long long clen,
                             const unsigned char *n,
                             const unsigned char *k);

int 
aespoly1305_afternm_cuda(unsigned char *c,
						 const unsigned char *m,unsigned long long mlen,
						 const unsigned char *n,
						 const unsigned char *k);
int 
aespoly1305_open_afternm_cuda(unsigned char *m,
							  const unsigned char *c,unsigned long long clen,
							  const unsigned char *n,
							  const unsigned char *k);


#ifdef __cplusplus
}
#endif


#endif

