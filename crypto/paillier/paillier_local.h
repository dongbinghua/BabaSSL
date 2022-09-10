/*
 * Copyright 2022 The Tongsuo Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/Tongsuo-Project/Tongsuo/blob/master/LICENSE.txt
 */

#ifndef HEADER_PAILLIER_LOCAL_H
# define HEADER_PAILLIER_LOCAL_H

# include <openssl/opensslconf.h>

# ifndef OPENSSL_NO_PAILLIER_LOCAL
# ifdef  __cplusplus
extern "C" {
# endif

# include <openssl/paillier.h>
# include "internal/refcount.h"

#if 0
struct paillier_pub_key_st {
    BIGNUM *n;
    BIGNUM *n_square;
    BIGNUM *g;
};

struct paillier_priv_key_st {
    BIGNUM *lambda;
    BIGNUM *u;
};
#endif

struct paillier_key_st {
    int32_t version;
    BIGNUM *p;
    BIGNUM *q;
    BIGNUM *n;
    BIGNUM *n_square;
    BIGNUM *g;
    BIGNUM *lambda;
    BIGNUM *u;
    CRYPTO_REF_COUNT references;
    CRYPTO_RWLOCK *lock;
};

struct paillier_ciphertext_st {
    BIGNUM *data;
};

struct paillier_ctx_st {
    PAILLIER_KEY *key;
};

int paillier_g_check(BIGNUM *g, BIGNUM *n_square, BN_CTX *ctx);
int paillier_lambda_calc(BIGNUM *out, BIGNUM *p, BIGNUM *q, BN_CTX *ctx);
int paillier_lambda_check(BIGNUM *lambda, BIGNUM *n, BN_CTX *ctx);
int paillier_l_func(BIGNUM *out, BIGNUM *x, BIGNUM *n, BN_CTX *ctx);
int ossl_paillier_multip_calc_product(PAILLIER_KEY *pail);

# ifdef  __cplusplus
}
# endif
# endif

#endif
