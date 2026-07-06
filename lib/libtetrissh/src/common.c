/**
 * common.c
 * --------
 * Implementation of shared utilities declared in common.h.
 *
 * Compile with: -lssl -lcrypto
 */

#include "libs/common.h"

/* ======================================================================
 * Integer <-> byte conversion
 * ====================================================================== */

void int_to_bytes(uint64_t x, unsigned char buf[INT_BYTES])
{
    /* Big-endian encoding */
    for (int i = INT_BYTES - 1; i >= 0; i--)
    {
        buf[i] = (unsigned char)(x & 0xFF);
        x >>= 8;
    }
}

uint64_t bytes_to_int(const unsigned char buf[INT_BYTES])
{
    /* Big-endian decoding */
    uint64_t result = 0;
    for (int i = 0; i < INT_BYTES; i++)
    {
        result = (result << 8) | buf[i];
    }
    return result;
}

/* ======================================================================
 * Socket helpers
 * ====================================================================== */

unsigned char *read_bytes(int sockfd, uint64_t length)
{
    /**
     * Reads exactly 'length' bytes from the socket.
     */
    unsigned char *buffer = malloc(length);
    if (!buffer)
    {
        perror("malloc");
        return NULL;
    }

    uint64_t bytes_received = 0;
    while (bytes_received < length)
    {
        /* Read in chunks of up to 1024 bytes */
        uint64_t remaining = length - bytes_received;
        size_t chunk = remaining < 1024 ? (size_t)remaining : 1024;
        ssize_t n = recv(sockfd, buffer + bytes_received, chunk, 0);
        if (n <= 0)
        {
            fprintf(stderr, "Socket connection broken\n");
            free(buffer);
            return NULL;
        }
        bytes_received += (uint64_t)n;
    }
    return buffer;
}

int send_all(int sockfd, const unsigned char *buf, uint64_t length)
{
    /**
     * Sends exactly 'length' bytes over the socket.
     */
    uint64_t total_sent = 0;
    while (total_sent < length)
    {
        ssize_t n = send(sockfd, buf + total_sent, (size_t)(length - total_sent), 0);
        if (n <= 0)
        {
            perror("send");
            return -1;
        }
        total_sent += (uint64_t)n;
    }
    return 0;
}

int send_int(int sockfd, uint64_t value)
{
    /** Convenience: encode integer as 8 big-endian bytes and send. */
    unsigned char buf[INT_BYTES];
    int_to_bytes(value, buf);
    return send_all(sockfd, buf, INT_BYTES);
}

/* ======================================================================
 * OpenSSL key/cert loading
 * ====================================================================== */

EVP_PKEY *load_private_key(const char *filename)
{
    /**
     * Reads a PEM private key file and returns an EVP_PKEY*.
     */
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "Cannot open private key file: %s\n", filename);
        return NULL;
    }
    EVP_PKEY *pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    fclose(fp);
    if (!pkey)
    {
        print_ssl_error("load_private_key");
    }
    return pkey;
}

X509 *load_cert_file(const char *filename)
{
    /**
     * Loads an X.509 certificate from a PEM file on disk.
     */
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "Cannot open certificate file: %s\n", filename);
        return NULL;
    }
    X509 *cert = PEM_read_X509(fp, NULL, NULL, NULL);
    fclose(fp);
    if (!cert)
    {
        print_ssl_error("load_cert_file");
    }
    return cert;
}

X509 *load_cert_bytes(const unsigned char *data, int len)
{
    /**
     * Parses an X.509 certificate from a PEM-encoded byte buffer received
     * over the network.
     */
    BIO *bio = BIO_new_mem_buf(data, len);
    if (!bio)
        return NULL;
    X509 *cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (!cert)
    {
        print_ssl_error("load_cert_bytes");
    }
    return cert;
}

/* ======================================================================
 * Certificate verification
 * ====================================================================== */

int verify_server_cert(X509 *server_cert, const char *ca_cert_path)
{
    /**
     * Verifies that server_cert was signed by the CA, and checks validity.
     *
     *   1. Loads the CA cert
     *   2. Uses ca_public_key.verify() to check the server cert signature
     *   3. Checks not_valid_before <= now <= not_valid_after
     */
    int ret = 0;

    /* Load CA certificate */
    X509 *ca_cert = load_cert_file(ca_cert_path);
    if (!ca_cert)
        return 0;

    /* Print validity period  */
    BIO *bio_out = BIO_new_fp(stdout, BIO_NOCLOSE);
    if (bio_out)
    {
        printf("Server cert not valid before: ");
        ASN1_TIME_print(bio_out, X509_get0_notBefore(server_cert));
        printf("\n");
        printf("Server cert not valid after:  ");
        ASN1_TIME_print(bio_out, X509_get0_notAfter(server_cert));
        printf("\n");
        BIO_free(bio_out);
    }

    /* Create a certificate store and add the CA cert as trusted */
    X509_STORE *store = X509_STORE_new();
    if (!store)
        goto cleanup;
    X509_STORE_add_cert(store, ca_cert);

    /* Create a verification context */
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    if (!ctx)
    {
        X509_STORE_free(store);
        goto cleanup;
    }

    X509_STORE_CTX_init(ctx, store, server_cert, NULL);

    /* Verify the certificate chain (checks signature AND validity period) */
    if (X509_verify_cert(ctx) == 1)
    {
        printf("Server certificate verified successfully.\n");
        ret = 1;
    }
    else
    {
        int err = X509_STORE_CTX_get_error(ctx);
        fprintf(stderr, "Certificate verification failed: %s\n",
                X509_verify_cert_error_string(err));
    }

    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);

cleanup:
    X509_free(ca_cert);
    return ret;
}

/* ======================================================================
 * RSA-PSS signing and verification
 * ====================================================================== */

unsigned char *sign_message_pss(EVP_PKEY *priv_key,
                                const unsigned char *msg, size_t msg_len,
                                size_t *sig_len)
{
    /**
     * Signs a message using RSA-PSS with SHA-256 and maximum salt length.
     */
    unsigned char *sig = NULL;
    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    if (!md_ctx)
        return NULL;

    EVP_PKEY_CTX *pkey_ctx = NULL;

    if (EVP_DigestSignInit(md_ctx, &pkey_ctx, EVP_sha256(), NULL, priv_key) <= 0)
        goto fail;

    /* Set PSS padding with MGF1-SHA256 and max salt */
    if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) <= 0)
        goto fail;
    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, RSA_PSS_SALTLEN_MAX) <= 0)
        goto fail;

    if (EVP_DigestSignUpdate(md_ctx, msg, msg_len) <= 0)
        goto fail;

    /* Determine signature length */
    if (EVP_DigestSignFinal(md_ctx, NULL, sig_len) <= 0)
        goto fail;

    sig = malloc(*sig_len);
    if (!sig)
        goto fail;

    if (EVP_DigestSignFinal(md_ctx, sig, sig_len) <= 0)
    {
        free(sig);
        sig = NULL;
        goto fail;
    }

fail:
    if (!sig)
        print_ssl_error("sign_message_pss");
    EVP_MD_CTX_free(md_ctx);
    return sig;
}

int verify_message_pss(X509 *cert,
                       const unsigned char *sig, size_t sig_len,
                       const unsigned char *msg, size_t msg_len)
{
    /**
     * Verifies an RSA-PSS signature.
     */
    int ret = 0;
    EVP_PKEY *pub_key = X509_get_pubkey(cert);
    if (!pub_key)
        return 0;

    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    if (!md_ctx)
    {
        EVP_PKEY_free(pub_key);
        return 0;
    }

    EVP_PKEY_CTX *pkey_ctx = NULL;

    if (EVP_DigestVerifyInit(md_ctx, &pkey_ctx, EVP_sha256(), NULL, pub_key) <= 0)
        goto done;
    if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) <= 0)
        goto done;
    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, RSA_PSS_SALTLEN_MAX) <= 0)
        goto done;
    if (EVP_DigestVerifyUpdate(md_ctx, msg, msg_len) <= 0)
        goto done;

    ret = (EVP_DigestVerifyFinal(md_ctx, sig, sig_len) == 1) ? 1 : 0;

done:
    if (!ret)
        print_ssl_error("verify_message_pss");
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pub_key);
    return ret;
}

/* ======================================================================
 * RSA encryption / decryption
 * ====================================================================== */

unsigned char *rsa_encrypt_block(EVP_PKEY *pub_key,
                                 const unsigned char *plain, size_t plain_len,
                                 size_t *out_len, int use_oaep)
{
    /**
     * Encrypts a single plaintext block with RSA.
     *
     * use_oaep=1: OAEP with SHA-256 (max 62 bytes for 1024-bit key)
     * use_oaep=0: PKCS1v15 (max 117 bytes for 1024-bit key)
     *
     */
    unsigned char *out = NULL;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pub_key, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_encrypt_init(ctx) <= 0)
        goto fail;

    if (use_oaep)
    {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
            goto fail;
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0)
            goto fail;
        if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0)
            goto fail;
    }
    else
    {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0)
            goto fail;
    }

    /* Determine output size */
    if (EVP_PKEY_encrypt(ctx, NULL, out_len, plain, plain_len) <= 0)
        goto fail;

    out = malloc(*out_len);
    if (!out)
        goto fail;

    if (EVP_PKEY_encrypt(ctx, out, out_len, plain, plain_len) <= 0)
    {
        free(out);
        out = NULL;
    }

fail:
    if (!out)
        print_ssl_error("rsa_encrypt_block");
    EVP_PKEY_CTX_free(ctx);
    return out;
}

unsigned char *rsa_decrypt_block(EVP_PKEY *priv_key,
                                 const unsigned char *cipher, size_t cipher_len,
                                 size_t *out_len, int use_oaep)
{
    /**
     * Decrypts a single RSA-encrypted block.
     */
    unsigned char *out = NULL;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(priv_key, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_decrypt_init(ctx) <= 0)
        goto fail;

    if (use_oaep)
    {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
            goto fail;
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0)
            goto fail;
        if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0)
            goto fail;
    }
    else
    {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0)
            goto fail;
    }

    /* Determine output size */
    if (EVP_PKEY_decrypt(ctx, NULL, out_len, cipher, cipher_len) <= 0)
        goto fail;

    out = malloc(*out_len);
    if (!out)
        goto fail;

    if (EVP_PKEY_decrypt(ctx, out, out_len, cipher, cipher_len) <= 0)
    {
        free(out);
        out = NULL;
    }

fail:
    if (!out)
        print_ssl_error("rsa_decrypt_block");
    EVP_PKEY_CTX_free(ctx);
    return out;
}

/* ======================================================================
 * Symmetric encryption
 * ======================================================================
 */

int generate_session_key(unsigned char key_out[SESSION_KEY_LEN])
{
    /** Generate a cryptographically secure random session key. */
    if (RAND_bytes(key_out, SESSION_KEY_LEN) != 1)
    {
        print_ssl_error("generate_session_key");
        return -1;
    }
    return 0;
}

unsigned char *session_encrypt(const unsigned char key[SESSION_KEY_LEN],
                               const unsigned char *plain, size_t plain_len,
                               size_t *out_len)
{
    /**
     * Encrypts plaintext using AES-128-CBC with PKCS7 padding, then appends
     * an HMAC-SHA256 over (IV || ciphertext).
     *
     * Output layout: IV (16) || ciphertext || HMAC (32)
     */
    const unsigned char *hmac_key = key;               /* first 16 bytes */
    const unsigned char *aes_key = key + HMAC_KEY_LEN; /* last 16 bytes */

    /* Generate random IV */
    unsigned char iv[AES_IV_LEN];
    if (RAND_bytes(iv, AES_IV_LEN) != 1)
    {
        print_ssl_error("session_encrypt: RAND_bytes");
        return NULL;
    }

    /* Allocate output: IV + ciphertext (at most plain_len + AES_BLOCK for padding) + HMAC */
    size_t max_ct_len = plain_len + AES_BLOCK; /* worst case with padding */
    size_t max_out = AES_IV_LEN + max_ct_len + HMAC_LEN;
    unsigned char *output = malloc(max_out);
    if (!output)
        return NULL;

    /* Copy IV to start of output */
    memcpy(output, iv, AES_IV_LEN);

    /* Encrypt with AES-128-CBC (OpenSSL applies PKCS7 padding by default) */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        free(output);
        return NULL;
    }

    int ct_len = 0, final_len = 0;
    unsigned char *ct_start = output + AES_IV_LEN;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, aes_key, iv) != 1 ||
        EVP_EncryptUpdate(ctx, ct_start, &ct_len, plain, (int)plain_len) != 1 ||
        EVP_EncryptFinal_ex(ctx, ct_start + ct_len, &final_len) != 1)
    {
        print_ssl_error("session_encrypt");
        EVP_CIPHER_CTX_free(ctx);
        free(output);
        return NULL;
    }
    EVP_CIPHER_CTX_free(ctx);

    size_t total_ct = (size_t)(ct_len + final_len);

    /* Compute HMAC-SHA256 over (IV || ciphertext) */
    unsigned int hmac_out_len = 0;
    unsigned char *hmac_ptr = output + AES_IV_LEN + total_ct;
    if (!HMAC(EVP_sha256(), hmac_key, HMAC_KEY_LEN,
              output, AES_IV_LEN + total_ct,
              hmac_ptr, &hmac_out_len))
    {
        print_ssl_error("session_encrypt: HMAC");
        free(output);
        return NULL;
    }

    *out_len = AES_IV_LEN + total_ct + HMAC_LEN;
    return output;
}

unsigned char *session_decrypt(const unsigned char key[SESSION_KEY_LEN],
                               const unsigned char *token, size_t token_len,
                               size_t *out_len)
{
    /**
     * Decrypts a token produced by session_encrypt().
     * Verifies HMAC first, then decrypts AES-128-CBC.
     */
    const unsigned char *hmac_key = key;
    const unsigned char *aes_key = key + HMAC_KEY_LEN;

    /* Minimum token size: IV (16) + at least 1 block ciphertext (16) + HMAC (32) = 64 */
    if (token_len < AES_IV_LEN + AES_BLOCK + HMAC_LEN)
    {
        fprintf(stderr, "session_decrypt: token too short\n");
        return NULL;
    }

    size_t ct_len = token_len - AES_IV_LEN - HMAC_LEN;
    const unsigned char *iv = token;
    const unsigned char *ct = token + AES_IV_LEN;
    const unsigned char *hmac_received = token + AES_IV_LEN + ct_len;

    /* Verify HMAC over (IV || ciphertext) */
    unsigned char hmac_computed[HMAC_LEN];
    unsigned int hmac_out_len = 0;
    if (!HMAC(EVP_sha256(), hmac_key, HMAC_KEY_LEN,
              token, AES_IV_LEN + ct_len,
              hmac_computed, &hmac_out_len))
    {
        print_ssl_error("session_decrypt: HMAC compute");
        return NULL;
    }
    if (CRYPTO_memcmp(hmac_computed, hmac_received, HMAC_LEN) != 0)
    {
        fprintf(stderr, "session_decrypt: HMAC verification failed!\n");
        return NULL;
    }

    /* Decrypt AES-128-CBC */
    unsigned char *plain = malloc(ct_len); /* at most ct_len bytes after removing padding */
    if (!plain)
        return NULL;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        free(plain);
        return NULL;
    }

    int pt_len = 0, final_len = 0;
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, aes_key, iv) != 1 ||
        EVP_DecryptUpdate(ctx, plain, &pt_len, ct, (int)ct_len) != 1 ||
        EVP_DecryptFinal_ex(ctx, plain + pt_len, &final_len) != 1)
    {
        print_ssl_error("session_decrypt");
        EVP_CIPHER_CTX_free(ctx);
        free(plain);
        return NULL;
    }
    EVP_CIPHER_CTX_free(ctx);

    *out_len = (size_t)(pt_len + final_len);
    return plain;
}

/* ======================================================================
 * Utility
 * ====================================================================== */

void print_ssl_error(const char *context)
{
    unsigned long err = ERR_get_error();
    if (err)
    {
        char buf[256];
        ERR_error_string_n(err, buf, sizeof(buf));
        fprintf(stderr, "[OpenSSL] %s: %s\n", context, buf);
    }
}

double get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
