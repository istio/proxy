#!/usr/bin/env bash
set -euo pipefail

# Uploads a local artifact file to gs://maistra-prow-testing/proxy/.
# Required env vars: GCS_SERVICE_ACCOUNT_KEY
# Optional env vars: DRY_RUN (default: false)
# Usage: upload-to-gcs.sh <artifact-name>

ARTIFACT_NAME="${1:?Usage: upload-to-gcs.sh <artifact-name>}"
export ARTIFACT_NAME

if [[ "${DRY_RUN:-false}" == "true" ]]; then
  echo "DRY RUN: would upload ${ARTIFACT_NAME} to gs://maistra-prow-testing/proxy/"
  echo "Artifact size: $(du -sh "${ARTIFACT_NAME}" | cut -f1)"
  exit 0
fi

echo "${GCS_SERVICE_ACCOUNT_KEY}" > /tmp/gcs-key.json
pip install --quiet google-cloud-storage
python3 - <<'EOF'
import os
from google.cloud import storage

client = storage.Client.from_service_account_json('/tmp/gcs-key.json')
bucket = client.bucket('maistra-prow-testing')
artifact = os.environ['ARTIFACT_NAME']
blob = bucket.blob(f'proxy/{artifact}')
blob.upload_from_filename(artifact)
url = f'https://storage.googleapis.com/maistra-prow-testing/proxy/{artifact}'
print(f'Uploaded: {url}')
EOF
rm -f /tmp/gcs-key.json
