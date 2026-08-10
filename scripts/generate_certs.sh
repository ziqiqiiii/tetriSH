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
# key with no passphrase. Only the two certificates are committed; the keys are
# local, and none of it may be used for anything but local runs, the demo
# server, and integration tests.
#
# Re-running is a no-op while the existing server certificate is still valid;
# pass FORCE=1 to regenerate unconditionally.
#
# The CA is never replaced when it can be kept. ca.crt is committed, so every
# client in the wild trusts exactly that CA, and minting a new one is not a
# refresh - it is a revocation of everybody. So an existing CA is reused to sign
# a new leaf, and a CA whose private key is absent is left untouched: that
# checkout cannot sign for it and is a client, not a server.

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

# Whether a certificate and a private key are actually the same keypair. The
# existence checks below cannot answer this, and nothing downstream asks:
# session_credentials_load reads the certificate and parses the key without ever
# calling X509_check_private_key, so a mismatched pair boots a server that looks
# healthy and then fails every single handshake. That is exactly what a checkout
# holds after it gains the committed server.crt while keeping an older local
# server.key, so the guard has to test the pairing and not the filenames.
pair_matches() {
	local crt_mod key_mod

	crt_mod=$(openssl x509 -noout -modulus -in "$1" 2>/dev/null) || return 1
	key_mod=$(openssl rsa -noout -modulus -in "$2" 2>/dev/null) || return 1
	[ -n "$crt_mod" ] && [ "$crt_mod" = "$key_mod" ]
}

# Still-valid certificates are left alone: regenerating would invalidate every
# client that already trusts the current CA.
if [ "$FORCE" != "1" ] && [ -f "$OUT_DIR/server.crt" ] \
	&& [ -f "$OUT_DIR/server.key" ] && [ -f "$OUT_DIR/ca.crt" ] \
	&& pair_matches "$OUT_DIR/server.crt" "$OUT_DIR/server.key" \
	&& openssl x509 -checkend 86400 -noout -in "$OUT_DIR/server.crt" >/dev/null 2>&1
then
	echo "${GRN}Certificates ${RST}in ${BLU}${OUT_DIR}${RST} are valid ✔️"
	exit 0
fi

mkdir -p "$OUT_DIR"
SUBJ_BASE="/C=SG/ST=Singapore/L=Singapore/O=SUTD"

# One question decides what happens to the CA: can this checkout sign for the
# ca.crt it already has? Only a matching ca.key can, and a key that is merely
# *present* is not enough - a stale local ca.key left over from an earlier CA
# would otherwise send this down the mint path and overwrite the committed
# certificate, which is the outcome the whole guard exists to prevent.
if [ -f "$OUT_DIR/ca.crt" ] && [ -f "$OUT_DIR/ca.key" ] \
	&& pair_matches "$OUT_DIR/ca.crt" "$OUT_DIR/ca.key"
then
	CA_MINE=1
else
	CA_MINE=0
fi

# An existing CA this checkout cannot sign for is somebody else's - the
# committed one - so it is left exactly as it is rather than replaced. That
# checkout is a client: it can verify the demo server and not be one. FORCE=1 is
# the deliberate override, and it is destructive to every client in the wild.
if [ "$FORCE" != "1" ] && [ -f "$OUT_DIR/ca.crt" ] && [ "$CA_MINE" -eq 0 ]
then
	echo "${GRN}CA ${RST}in ${BLU}${OUT_DIR}/ca.crt${RST} is not ours to reissue; leaving it alone ✔️"
	echo "No matching ${BLU}ca.key${RST} here, so this checkout can verify that CA but not sign for it:"
	echo "  play against the demo server with ${YEL}TETRISU_CA_PATH=$OUT_DIR/ca.crt${RST}"
	echo "  to run a server here instead, mint a fresh CA with ${YEL}FORCE=1${RST} - that CA is"
	echo "  trusted by nobody else, and replaces the committed one for every client"
	exit 0
fi

echo "${YEL}Generating ${RST}dev CA and server certificate in ${BLU}${OUT_DIR}${RST}..."

# Reuse the CA when this checkout owns it, so the committed ca.crt keeps its
# meaning and only the leaf is reissued.
if [ "$FORCE" != "1" ] && [ "$CA_MINE" -eq 1 ]
then
	echo "${GRN}Reusing ${RST}the CA in ${BLU}${OUT_DIR}/ca.crt${RST}."
else
	openssl genrsa -out "$OUT_DIR/ca.key" "$BITS" >/dev/null 2>&1
	openssl req -x509 -new -nodes -key "$OUT_DIR/ca.key" -sha256 -days "$DAYS" \
		-subj "${SUBJ_BASE}/CN=tetrish-dev-ca" \
		-out "$OUT_DIR/ca.crt" >/dev/null 2>&1
fi

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
