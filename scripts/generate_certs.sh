#!/usr/bin/env bash
# Generate the development CA and server certificate tetrisd boots with.
#
# Usage:  scripts/generate_certs.sh [out_dir]        (default: ./certs)
#
# The secure session (libtetrissh) authenticates the *server* to the client:
# tetrisd signs the client nonce with its private key and ships its
# certificate; the client verifies that chain against the CA. So a fresh
# clone needs three files before `make stack` can work — ca.crt, server.crt,
# server.key — and this script mints them.
#
# These are DEVELOPMENT credentials: a self-signed CA, ~365-day leaf, and a
# key with no passphrase. They live in a git-ignored directory and must never
# be used for anything but local runs and integration tests.
#
# Re-running is a no-op while the existing server certificate is still valid;
# pass FORCE=1 to regenerate unconditionally.

set -euo pipefail

OUT_DIR="${1:-certs}"
DAYS="${DAYS:-365}"
BITS="${BITS:-2048}"
FORCE="${FORCE:-0}"

GRN=$(printf '\033[1;32m')
YEL=$(printf '\033[1;33m')
BLU=$(printf '\033[1;34m')
RST=$(printf '\033[0m')

if ! command -v openssl >/dev/null 2>&1; then
	echo "generate_certs.sh: openssl not found — run 'make deps'" >&2
	exit 1
fi

# Still-valid certificates are left alone: regenerating would invalidate every
# client that already trusts the current CA.
if [ "$FORCE" != "1" ] && [ -f "$OUT_DIR/server.crt" ] \
	&& [ -f "$OUT_DIR/server.key" ] && [ -f "$OUT_DIR/ca.crt" ] \
	&& openssl x509 -checkend 86400 -noout -in "$OUT_DIR/server.crt" >/dev/null 2>&1
then
	echo "${GRN}Certificates ${RST}in ${BLU}${OUT_DIR}${RST} are valid ✔️"
	exit 0
fi

mkdir -p "$OUT_DIR"
SUBJ_BASE="/C=SG/ST=Singapore/L=Singapore/O=SUTD"

echo "${YEL}Generating ${RST}dev CA and server certificate in ${BLU}${OUT_DIR}${RST}..."

openssl genrsa -out "$OUT_DIR/ca.key" "$BITS" >/dev/null 2>&1
openssl req -x509 -new -nodes -key "$OUT_DIR/ca.key" -sha256 -days "$DAYS" \
	-subj "${SUBJ_BASE}/CN=tetrish-dev-ca" \
	-out "$OUT_DIR/ca.crt" >/dev/null 2>&1

openssl genrsa -out "$OUT_DIR/server.key" "$BITS" >/dev/null 2>&1
openssl req -new -key "$OUT_DIR/server.key" \
	-subj "${SUBJ_BASE}/CN=tetrisd" \
	-out "$OUT_DIR/server.csr" >/dev/null 2>&1
openssl x509 -req -in "$OUT_DIR/server.csr" -CA "$OUT_DIR/ca.crt" \
	-CAkey "$OUT_DIR/ca.key" -CAcreateserial -days "$DAYS" -sha256 \
	-out "$OUT_DIR/server.crt" >/dev/null 2>&1

rm -f "$OUT_DIR/server.csr" "$OUT_DIR/ca.srl"
chmod 600 "$OUT_DIR/ca.key" "$OUT_DIR/server.key"

echo "${GRN}Created ${BLU}${OUT_DIR}/{ca.crt,server.crt,server.key}${RST} ✔️"
