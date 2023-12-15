#!/usr/bin/env bash

set -e -o pipefail

DB_ID="MiSTerLaggy"
TIMESTAMP=$(date +%s)
BASE_URL="https://raw.githubusercontent.com/MiSTer-devel/MiSTerLaggy_MiSTer/main/releases"

# shellcheck disable=SC2012
LATEST_FILE=$(ls -t -- *.rbf | head -1)

HASH=$(md5 -q "$LATEST_FILE")
SIZE=$(wc -c < "$LATEST_FILE")
URL="$BASE_URL/$LATEST_FILE"
FILE="_Utility/$LATEST_FILE"


DATABASE=$(jq --null-input \
  --arg db_id "$DB_ID" \
  --arg timestamp "$TIMESTAMP" \
  --arg file "$FILE" \
  --arg hash "$HASH" \
  --arg size "$SIZE" \
  --arg url "$URL" \
  --indent 4 \
'{
    "db_id": $db_id,
    "timestamp": $timestamp|tonumber,
    "files": {
        $file: {
            "hash": $hash,
            "size": $size|tonumber,
            "url": $url,
            "reboot": false,
            "tags": []
        }
    },
    "folders": {
        "_Utility": {

        }
    }
}
'
)

echo "$DATABASE" > db.json