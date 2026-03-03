#!/bin/bash
set -euo pipefail

sha512sum *.gz *.zip *.exe > checksums-sha512.txt

# https://docs.sigstore.dev/quickstart/quickstart-cosign/
cosign sign-blob checksums-sha512.txt --bundle cosign.bundle

cosign verify-blob checksums-sha512.txt --bundle cosign.bundle \
    --certificate-identity=hluk@email.cz \
    --certificate-oidc-issuer=https://github.com/login/oauth
