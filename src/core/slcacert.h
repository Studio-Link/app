/**
 * @file slcacert.h Studio Link embedded CA certificates
 *
 * The Mozilla CA bundle from https://curl.se/ca/cacert.pem is embedded at
 * build time (see SL_CACERT_* in CMakeLists.txt), so TLS verification does
 * not depend on the system or on the build path of the static OpenSSL.
 *
 * Copyright (C) 2026 studio-link.de
 */
#ifndef SLCACERT_H
#define SLCACERT_H

struct tls;
struct http_cli;
struct dnsc;

/** The embedded CA bundle (PEM, 0-terminated) */
const char *sl_cacert_pem(void);

/** Add all embedded CA certificates to a TLS context */
int sl_cacert_add(struct tls *tls);

/** http_client_alloc() with the embedded CA certificates */
int sl_http_client_alloc(struct http_cli **clip, struct dnsc *dnsc);

#endif /* SLCACERT_H */
