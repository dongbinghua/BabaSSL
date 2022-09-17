/*
 * Copyright 2022 The Tongsuo Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/Tongsuo-Project/Tongsuo/blob/master/LICENSE.txt
 */

#include <openssl/err.h>
#include "paillier_local.h"

PAILLIER_KEY *PAILLIER_KEY_new()
{
    PAILLIER_KEY *key = NULL;

    key = OPENSSL_zalloc(sizeof(*key));
    if (key == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    key->n = BN_new();
    if (key->n == NULL)
        goto err;

    key->n_square = BN_new();
    if (key->n_square == NULL)
        goto err;

    key->g = BN_new();
    if (key->g == NULL)
        goto err;

    key->lambda = BN_new();
    if (key->lambda == NULL)
        goto err;

    key->u = BN_new();
    if (key->u == NULL)
        goto err;

    key->references = 1;
    key->lock = CRYPTO_THREAD_lock_new();
    if (key->lock == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    return key;
err:
    PAILLIER_KEY_free(key);
    return NULL;
}

void PAILLIER_KEY_free(PAILLIER_KEY *key)
{
    int i;

    if (key == NULL)
        return;

    CRYPTO_DOWN_REF(&key->references, &i, key->lock);
    REF_PRINT_COUNT("PAILLIER_KEY", key);
    if (i > 0)
        return;
    REF_ASSERT_ISNT(i < 0);

    BN_free(key->p);
    BN_free(key->q);
    BN_free(key->n);
    BN_free(key->n_square);
    BN_free(key->g);
    BN_free(key->lambda);
    BN_free(key->u);

    CRYPTO_THREAD_lock_free(key->lock);

    OPENSSL_clear_free((void *)key, sizeof(PAILLIER_KEY));
}

PAILLIER_KEY *PAILLIER_KEY_copy(PAILLIER_KEY *dest, PAILLIER_KEY *src)
{
    if (dest == NULL || src == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }

    if (dest == src)
        return dest;

    dest->version = src->version;

    dest->p = (dest->p ? BN_copy(dest->p, src->p) : BN_dup(src->p));
    dest->q = (dest->q ? BN_copy(dest->q, src->q) : BN_dup(src->q));

    if (!dest->p || !dest->q
        || !BN_copy(dest->n, src->n)
        || !BN_copy(dest->n_square, src->n_square)
        || !BN_copy(dest->g, src->g)
        || !BN_copy(dest->lambda, src->lambda)
        || !BN_copy(dest->u, src->u))
        return NULL;

    return dest;
}

PAILLIER_KEY *PAILLIER_KEY_dup(PAILLIER_KEY *key)
{
    PAILLIER_KEY *ret = NULL;

    if (key == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }

    ret = OPENSSL_zalloc(sizeof(*ret));
    if (ret == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    ret->p = BN_dup(key->p);
    if (ret->p == NULL)
        goto err;

    ret->q = BN_dup(key->q);
    if (ret->q == NULL)
        goto err;

    ret->n = BN_dup(key->n);
    if (ret->n == NULL)
        goto err;

    ret->n_square = BN_dup(key->n_square);
    if (ret->n_square == NULL)
        goto err;

    ret->g = BN_dup(key->g);
    if (ret->g == NULL)
        goto err;

    ret->lambda = BN_dup(key->lambda);
    if (ret->lambda == NULL)
        goto err;

    ret->u = BN_dup(key->u);
    if (ret->u == NULL)
        goto err;

    return ret;
err:
    OPENSSL_free(ret);
    return NULL;
}

int PAILLIER_KEY_up_ref(PAILLIER_KEY *key)
{
    int i;

    if (CRYPTO_UP_REF(&key->references, &i, key->lock) <= 0)
        return 0;

    REF_PRINT_COUNT("PAILLIER_KEY", key);
    REF_ASSERT_ISNT(i < 2);
    return ((i > 1) ? 1 : 0);
}

int PAILLIER_KEY_generate_key(PAILLIER_KEY *key, int strength)
{
    int ret = 0;
    BIGNUM *p, *q, *g_exp_lambda;
    BN_CTX *bn_ctx = NULL;

    if (key == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_PASSED_NULL_PARAMETER);
        return ret;
    }

    bn_ctx = BN_CTX_new();
    if (bn_ctx == NULL)
        goto err;

    p = BN_CTX_get(bn_ctx);
    q = BN_CTX_get(bn_ctx);
    g_exp_lambda = BN_CTX_get(bn_ctx);
    if (g_exp_lambda == NULL)
        goto err;

    if (!BN_generate_prime_ex(p, strength, 1, NULL, NULL, NULL))
        goto err;

    if (!BN_generate_prime_ex(q, strength, 1, NULL, NULL, NULL))
        goto err;

    if (!BN_mul(key->n, p, q, bn_ctx))
        goto err;

    if (!BN_sqr(key->n_square, key->n, bn_ctx))
        goto err;

    if (!BN_add(key->g, key->n, BN_value_one()))
        goto err;

    if (!paillier_g_check(key->g, key->n_square, bn_ctx))
        goto err;

    if (!paillier_lambda_calc(key->lambda, p, q, bn_ctx))
        goto err;

    if (!BN_mod_exp(g_exp_lambda, key->g, key->lambda, key->n_square, bn_ctx))
        goto err;

    if (!paillier_l_func(key->u, g_exp_lambda, key->n, bn_ctx))
        goto err;

    if (!BN_mod_inverse(key->u, key->u, key->n, bn_ctx))
        goto err;

    key->version = PAILLIER_ASN1_VERSION_DEFAULT;
    key->p = BN_dup(p);
    key->q = BN_dup(q);
    key->flag |= PAILLIER_FLAG_G_OPTIMIZE;
    ret = 1;
err:
    BN_CTX_free(bn_ctx);
    return ret;
}

int PAILLIER_KEY_type(PAILLIER_KEY *key)
{
    if (key != NULL && key->p != NULL && key->q != NULL
        && key->lambda != NULL && key->u != NULL)
        return PAILLIER_KEY_TYPE_PRIVATE;

    return PAILLIER_KEY_TYPE_PUBLIC;
}

int paillier_g_check(BIGNUM *g, BIGNUM *n_square, BN_CTX *ctx)
{
    int ret = 0;
    BIGNUM *gcd;
    BN_CTX *bn_ctx = NULL;

    if (g == NULL || n_square == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_PASSED_NULL_PARAMETER);
        return ret;
    }

    if (ctx == NULL) {
        bn_ctx = ctx = BN_CTX_new();
        if (ctx == NULL)
            goto err;
    }

    gcd = BN_CTX_get(ctx);
    if (gcd == NULL)
        goto err;

    if (!BN_gcd(gcd, g, n_square, ctx))
        goto err;

    ret = BN_is_one(gcd);

err:
    BN_CTX_free(bn_ctx);
    return ret;
}

/*
 * lambda calc
 */
int paillier_lambda_calc(BIGNUM *out, BIGNUM *p, BIGNUM *q, BN_CTX *ctx)
{
    int ret = 0;
    BIGNUM *gcd, *p_1, *q_1, *pq, *lambda;
    BN_CTX *bn_ctx = NULL;

    if (p == NULL || q == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_PASSED_NULL_PARAMETER);
        return ret;
    }

    if (ctx == NULL) {
        bn_ctx = ctx = BN_CTX_new();
        if (ctx == NULL)
            goto err;
    }

    lambda = out ? out : BN_CTX_get(ctx);
    gcd = BN_CTX_get(ctx);
    p_1 = BN_CTX_get(ctx);
    q_1 = BN_CTX_get(ctx);
    pq = BN_CTX_get(ctx);
    if (pq == NULL)
        goto err;

    /* p_1 = p - 1 */
    if (!BN_sub(p_1, p, BN_value_one()))
        goto err;

    /* q_1 = q - 1 */
    if (!BN_sub(q_1, q, BN_value_one()))
        goto err;

    /* gcd = gcd(p - 1, q - 1) */
    if (!BN_gcd(gcd, p_1, q_1, ctx))
        goto err;

    /* pq = (p - 1) * (q - 1) */
    if (!BN_mul(pq, p_1, q_1, ctx))
        goto err;

    /* lambda = (p - 1) * (q - 1) / gcd */
    if (!BN_div(lambda, NULL, pq, gcd, ctx))
        goto err;

    if (!paillier_lambda_check(lambda, pq, ctx))
        goto err;

    ret = 1;

err:
    BN_CTX_free(bn_ctx);
    return ret;
}

/*
 * lambda check
 */
int paillier_lambda_check(BIGNUM *lambda, BIGNUM *n, BN_CTX *ctx)
{
    int ret = 0;
    BIGNUM *n_square, *n_lambda, *r, *r_exp_lambda, *r_exp_n_lambda;
    BN_CTX *bn_ctx = NULL;

    if (lambda == NULL || n == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_PASSED_NULL_PARAMETER);
        return ret;
    }

    if (ctx == NULL) {
        bn_ctx = ctx = BN_CTX_new();
        if (ctx == NULL)
            goto err;
    }

    n_square = BN_CTX_get(ctx);
    n_lambda = BN_CTX_get(ctx);
    r = BN_CTX_get(ctx);
    r_exp_lambda = BN_CTX_get(ctx);
    r_exp_n_lambda = BN_CTX_get(ctx);
    if (r_exp_n_lambda == NULL)
        goto err;

    /* n_square = n ^ 2 */
    if (!BN_sqr(n_square, n, ctx))
        goto err;

    /* n_lambda = n * lambda */
    if (!BN_mul(n_lambda, n, lambda, ctx))
        goto err;

    if (!BN_rand_range(r, n))
        goto err;

    /* r = r * n */
    if (!BN_mul(r, r, n, ctx))
        goto err;

    /* r = r - 1 */
    if (!BN_sub_word(r, 1))
        goto err;

    /* r_exp_lambda = (r ^ lambda) mod n */
    if (!BN_mod_exp(r_exp_lambda, r, lambda, n, ctx))
        goto err;

    /* r_exp_n_lambda = (r ^ n_lambda) mod n_square */
    if (!BN_mod_exp(r_exp_n_lambda, r, n_lambda, n_square, ctx))
        goto err;

    ret = BN_cmp(r_exp_lambda, r_exp_n_lambda) == 0;

err:
    BN_CTX_free(bn_ctx);
    return ret;
}

/*
 * The L function is used in decryption process and generate key.
 * L(x) = (x-1)/n
 * where x is an element of {x < n^2 | x = 1 mod n}
 * n is our modulus (p*q)
 */
int paillier_l_func(BIGNUM *out, BIGNUM *x, BIGNUM *n, BN_CTX *ctx)
{
    int ret = 0;
    BIGNUM *x_1;
    BN_CTX *bn_ctx = NULL;

    if (out == NULL || x == NULL || n == NULL) {
        ERR_raise(ERR_LIB_PAILLIER, ERR_R_PASSED_NULL_PARAMETER);
        return ret;
    }

    if (ctx == NULL) {
        bn_ctx = ctx = BN_CTX_new();
        if (ctx == NULL)
            goto err;
    }

    x_1 = BN_CTX_get(ctx);
    if (x_1 == NULL)
        goto err;

    /* x_1 = x - 1 */
    if (!BN_sub(x_1, x, BN_value_one()))
        goto err;

    /* out = (x - 1) / n */
    if (!BN_div(out, NULL, x_1, n, ctx))
        goto err;

    ret = 1;

err:
    BN_CTX_free(bn_ctx);
    return ret;
}

int ossl_paillier_multip_calc_product(PAILLIER_KEY *pail)
{
    /* TODO */
    return 1;
}
