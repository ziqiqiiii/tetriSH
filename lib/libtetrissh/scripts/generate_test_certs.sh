#!/bin/sh
set -eu

out_dir="${1:-tests/tmp/certs}"
mkdir -p "$out_dir"

openssl genrsa -out "$out_dir/ca.key" 1024 >/dev/null 2>&1
openssl req -x509 -new -nodes -key "$out_dir/ca.key" -sha256 -days 1 \
	-subj "/C=SG/ST=Singapore/L=Singapore/O=SUTD/CN=tetrish-test-ca" \
	-out "$out_dir/ca.crt" >/dev/null 2>&1

openssl genrsa -out "$out_dir/wrong_ca.key" 1024 >/dev/null 2>&1
openssl req -x509 -new -nodes -key "$out_dir/wrong_ca.key" -sha256 -days 1 \
	-subj "/C=SG/ST=Singapore/L=Singapore/O=SUTD/CN=tetrish-wrong-ca" \
	-out "$out_dir/wrong_ca.crt" >/dev/null 2>&1

openssl genrsa -out "$out_dir/server.key" 1024 >/dev/null 2>&1
openssl req -new -key "$out_dir/server.key" \
	-subj "/C=SG/ST=Singapore/L=Singapore/O=SUTD/CN=sutd.edu.sg" \
	-out "$out_dir/server.csr" >/dev/null 2>&1
openssl x509 -req -in "$out_dir/server.csr" -CA "$out_dir/ca.crt" \
	-CAkey "$out_dir/ca.key" -CAcreateserial -out "$out_dir/server.crt" \
	-days 1 -sha256 >/dev/null 2>&1
