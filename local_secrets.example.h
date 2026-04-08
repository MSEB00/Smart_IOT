#ifndef LOCAL_SECRETS_H
#define LOCAL_SECRETS_H

// Copy this file to local_secrets.h and fill your real values.
// local_secrets.h must stay local and must never be committed.

static const char* ssid = "YOUR_WIFI_SSID";
static const char* password = "YOUR_WIFI_PASSWORD";

static const char server_cert[] = R"crt(
-----BEGIN CERTIFICATE-----
PASTE_YOUR_CERT_PEM_HERE
-----END CERTIFICATE-----
)crt";

static const char server_key[] = R"key(
-----BEGIN RSA PRIVATE KEY-----
PASTE_YOUR_PRIVATE_KEY_PEM_HERE
-----END RSA PRIVATE KEY-----
)key";

#endif  // LOCAL_SECRETS_H
