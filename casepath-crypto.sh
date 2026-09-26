#!/usr/bin/env bash
set -euo pipefail

CERT="/home/we6jbo/.local/share/CasePath/keys/casepath_certificate.pem"
KEY="/home/we6jbo/.local/share/CasePath/keys/casepath_private_key.pem"

usage() {
    echo "Usage:"
    echo "  $0 encrypt INPUT OUTPUT"
    echo "  $0 decrypt INPUT OUTPUT"
}

[[ $# -eq 3 ]] || { usage; exit 2; }

mode="$1"
input="$2"
output="$3"

case "$mode" in
    encrypt)
        openssl cms -encrypt -binary -aes256 \
            -in "$input" \
            -out "$output" \
            -outform DER \
            "$CERT"
        ;;
    decrypt)
        openssl cms -decrypt -binary \
            -in "$input" \
            -inform DER \
            -recip "$CERT" \
            -inkey "$KEY" \
            -out "$output"
        ;;
    *)
        usage
        exit 2
        ;;
esac
