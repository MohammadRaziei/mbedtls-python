#!/usr/bin/env bash
# Regenerates tests/data/*.pem (certificates only, the private keys are thrown away). Needs OpenSSL >= 1.1.1.
#
#   root -> int -> leaf          EC P-256, the main chain
#   root -> int -> expired       same chain, valid for one day in 2020
#   other_root                   an unrelated root
#   rsa_root -> rsa_leaf         RSA 2048 chain
#
# Valid from 2020 until 2126, so the tests do not start failing next year.
set -euo pipefail
out="$(cd "$(dirname "$0")" && pwd)/data"
mkdir -p "$out"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

cat > ca.cnf <<'CNF'
[ca]
default_ca = CA_default
[CA_default]
database = index.txt
new_certs_dir = .
serial = serial.txt
default_md = sha256
policy = policy
unique_subject = no
[policy]
commonName = supplied
organizationName = optional
CNF
touch index.txt

ec()  { openssl ecparam -name prime256v1 -genkey -noout -out "$1.key"; }
rsa() { openssl genrsa -out "$1.key" 2048 2>/dev/null; }

# sign <name> <subject> <issuer-name|self> <serial-hex> <start> <end> <extensions...>
sign() {
    local name=$1 subject=$2 issuer=$3 serial=$4 start=$5 end=$6
    shift 6
    printf '[ext]\n%s\n' "$(printf '%s\n' "$@")" > "$name.ext"
    echo "$serial" > serial.txt
    openssl req -new -key "$name.key" -subj "$subject" -out "$name.csr"
    if [ "$issuer" = self ]; then
        openssl ca -config ca.cnf -batch -notext -selfsign -keyfile "$name.key" -in "$name.csr" \
            -startdate "$start" -enddate "$end" -extfile "$name.ext" -extensions ext -out "$name.pem" 2>/dev/null
    else
        openssl ca -config ca.cnf -batch -notext -cert "$issuer.pem" -keyfile "$issuer.key" -in "$name.csr" \
            -startdate "$start" -enddate "$end" -extfile "$name.ext" -extensions ext -out "$name.pem" 2>/dev/null
    fi
}

FROM=20200101000000Z
TO=21260101000000Z
CA_EXT=("basicConstraints=critical,CA:TRUE" "keyUsage=critical,keyCertSign,cRLSign")
INT_EXT=("basicConstraints=critical,CA:TRUE,pathlen:0" "keyUsage=critical,keyCertSign,cRLSign")
LEAF_EXT=("basicConstraints=CA:FALSE" "keyUsage=critical,digitalSignature" "subjectAltName=DNS:leaf.example.test,DNS:*.wild.example.test")

ec root;       sign root       "/O=mbedtls-python tests/CN=Test Root CA"         self 01 $FROM $TO "${CA_EXT[@]}"
ec int;        sign int        "/O=mbedtls-python tests/CN=Test Intermediate CA" root 02 $FROM $TO "${INT_EXT[@]}"
ec leaf;       sign leaf       "/CN=leaf.example.test"                           int  1234567890ABCDEF $FROM $TO "${LEAF_EXT[@]}"
ec expired;    sign expired    "/CN=expired.example.test"                        int  0A 20200101000000Z 20200102000000Z "${LEAF_EXT[@]}"
ec other_root; sign other_root "/CN=Other Root CA"                               self 03 $FROM $TO "${CA_EXT[@]}"
rsa rsa_root;  sign rsa_root   "/CN=RSA Test Root"                               self 04 $FROM $TO "${CA_EXT[@]}"
rsa rsa_leaf;  sign rsa_leaf   "/CN=rsa-leaf.example.test"                       rsa_root 05 $FROM $TO "${LEAF_EXT[@]}"

for n in root int leaf expired other_root rsa_root rsa_leaf; do cp "$n.pem" "$out/$n.pem"; done
echo "wrote $out"
