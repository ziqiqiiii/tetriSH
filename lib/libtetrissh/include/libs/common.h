/**
 * common.h
 * --------
 * Shared declarations for the Secure FTP project
 *
 * Every client and server source file includes this header. It provides:
 *   - Integer <-> big-endian byte conversion
 *   - Reliable socket read
 *   - OpenSSL convenience wrappers for signing, verifying, encrypting,
 * decrypting
 *   - Fernet-equivalent symmetric encrypt/decrypt (AES-128-CBC + HMAC-SHA256)
 *   - X.509 certificate loading and verification helpers
 */

#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

/* ======================================================================
 * Constants
 * ====================================================================== */

/** All length-prefix fields in the wire protocol are 8 bytes, big-endian. */
#define INT_BYTES 8

/** RSA key size used in this project (matches generate_keys.py: 1024-bit). */
#define RSA_KEY_BITS 1024
#define RSA_KEY_BYTES (RSA_KEY_BITS / 8) /* 128 bytes */

/**
 * Maximum plaintext that fits in one RSA-OAEP block with SHA-256:
 *   floor(n/8) - 2*ceil(h/8) - 2 = 128 - 2*32 - 2 = 62 bytes
 * For PKCS1v15: 128 - 11 = 117 bytes
 */
#define RSA_OAEP_CHUNK 62
#define RSA_PKCS1_CHUNK 117

/** AES-128-CBC block and key sizes (used by our Fernet-equivalent). */
#define AES_KEY_LEN 16
#define AES_IV_LEN 16
#define AES_BLOCK 16
#define HMAC_KEY_LEN 16
#define HMAC_LEN 32 /* SHA-256 output */

/**
 * Session key length = HMAC key (16) + AES key (16) = 32 bytes.
 */
#define SESSION_KEY_LEN (HMAC_KEY_LEN + AES_KEY_LEN)

/* ======================================================================
 * Protocol message types
 * ====================================================================== */
#define MSG_FILENAME 0
#define MSG_FILE_DATA 1
#define MSG_CLOSE 2
#define MSG_AUTH 3
#define MSG_SYMKEY 4 /* CP2 only: symmetric key exchange */

/* ======================================================================
 * Integer <-> byte conversion
 * ====================================================================== */

/**
 * Converts a 64-bit unsigned integer to an 8-byte big-endian buffer.
 */
void int_to_bytes(uint64_t x, unsigned char buf[INT_BYTES]);

/**
 * Converts an 8-byte big-endian buffer to a 64-bit unsigned integer.
 */
uint64_t bytes_to_int(const unsigned char buf[INT_BYTES]);

/* ======================================================================
 * Socket helpers
 * ====================================================================== */

/**
 * Reads exactly 'length' bytes from 'sockfd' into a newly malloc'd buffer.
 * Returns pointer to buffer on success, NULL on failure.
 * Caller must free() the returned buffer.
 */
unsigned char *read_bytes(int sockfd, uint64_t length);

/**
 * Sends exactly 'length' bytes from 'buf' over 'sockfd'.
 * Returns 0 on success, -1 on failure.
 */
int send_all(int sockfd, const unsigned char *buf, uint64_t length);

/**
 * Convenience: sends an 8-byte big-endian integer over the socket.
 */
int send_int(int sockfd, uint64_t value);

/* ======================================================================
 * OpenSSL key/cert loading
 * ====================================================================== */

/**
 * Loads an RSA private key from a PEM file.
 * Returns an EVP_PKEY* on success, NULL on failure.
 * Caller must EVP_PKEY_free() the returned key.
 */
EVP_PKEY *load_private_key(const char *filename);

/**
 * Loads an X.509 certificate from a PEM file on disk.
 * Returns an X509* on success, NULL on failure.
 * Caller must X509_free() the returned cert.
 */
X509 *load_cert_file(const char *filename);

/**
 * Parses an X.509 certificate from a PEM-encoded byte buffer.
 * Returns an X509* on success, NULL on failure.
 * Caller must X509_free() the returned cert.
 */
X509 *load_cert_bytes(const unsigned char *data, int len);

/* ======================================================================
 * Certificate verification
 * ====================================================================== */

/**
 * Verifies that 'server_cert' was signed by the CA whose certificate
 * is at 'ca_cert_path'. Also checks validity period.
 * Returns 1 on success, 0 on failure. Prints diagnostics.
 */
int verify_server_cert(X509 *server_cert, const char *ca_cert_path);

/* ======================================================================
 * RSA-PSS signing and verification (Authentication Protocol)
 * ====================================================================== */

/**
 * Signs 'msg' (of 'msg_len' bytes) with the private key using RSA-PSS
 * with SHA-256 and maximum salt length.
 *
 * Returns a newly malloc'd signature buffer, and writes its length to *sig_len.
 * Returns NULL on failure. Caller must free().
 */
unsigned char *sign_message_pss(EVP_PKEY *priv_key, const unsigned char *msg,
                                size_t msg_len, size_t *sig_len);

/**
 * Verifies an RSA-PSS signature on 'msg' using the public key from 'cert'.
 * Returns 1 if valid, 0 if invalid or error.
 */
int verify_message_pss(X509 *cert, const unsigned char *sig, size_t sig_len,
                       const unsigned char *msg, size_t msg_len);

/* ======================================================================
 * RSA encryption / decryption (CP1 and key exchange in CP2)
 * ====================================================================== */

/**
 * Encrypts a single block of plaintext with RSA-OAEP (SHA-256) or RSA-PKCS1v15.
 * 'use_oaep' selects the padding mode.
 *
 * Returns a newly malloc'd ciphertext buffer (RSA_KEY_BYTES long),
 * and writes its length to *out_len. Returns NULL on failure.
 */
unsigned char *rsa_encrypt_block(EVP_PKEY *pub_key, const unsigned char *plain,
                                 size_t plain_len, size_t *out_len,
                                 int use_oaep);

/**
 * Decrypts a single RSA-encrypted block.
 * Returns a newly malloc'd plaintext buffer, writes length to *out_len.
 * Returns NULL on failure.
 */
unsigned char *rsa_decrypt_block(EVP_PKEY *priv_key,
                                 const unsigned char *cipher, size_t cipher_len,
                                 size_t *out_len, int use_oaep);

/* ======================================================================
 * Symmetric encryption (CP2)
 * ======================================================================
 */

/**
 * Generates a random 32-byte session key.
 * Writes SESSION_KEY_LEN bytes into 'key_out'.
 * Returns 0 on success, -1 on failure.
 */
int generate_session_key(unsigned char key_out[SESSION_KEY_LEN]);

/**
 * Encrypts 'plain' using AES-128-CBC + HMAC-SHA256.
 * Layout of output: IV (16) || ciphertext || HMAC (32).
 *
 * Returns a newly malloc'd buffer and writes its length to *out_len.
 * Returns NULL on failure.
 */
unsigned char *session_encrypt(const unsigned char key[SESSION_KEY_LEN],
                               const unsigned char *plain, size_t plain_len,
                               size_t *out_len);

/**
 * Decrypts a token produced by session_encrypt().
 * Verifies HMAC first; returns NULL if verification fails.
 *
 * Returns a newly malloc'd plaintext buffer, writes length to *out_len.
 */
unsigned char *session_decrypt(const unsigned char key[SESSION_KEY_LEN],
                               const unsigned char *token, size_t token_len,
                               size_t *out_len);

/* ======================================================================
 * Utility
 * ====================================================================== */

/** Prints the most recent OpenSSL error to stderr. */
void print_ssl_error(const char *context);

/** Returns wall-clock time in seconds */
double get_time(void);

#endif /* COMMON_H */
