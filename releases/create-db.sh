#!/usr/bin/env bash

set -e -o pipefail

DB_ID="MiSTerLaggy"
TIMESTAMP=$(date +%s)
BASE_URL="https://raw.githubusercontent.com/MiSTer-devel/MiSTerLaggy_MiSTer/main/releases"

# TODO: this loop is frivolous and only ends up adding the latest release to the db
for rbf in *.rbf; do
    FILE=$rbf

    HASH=$(md5 -q "$rbf")
    SIZE=$(wc -c < "$rbf")
    URL="$BASE_URL/$rbf"
    FILE="_Utility/$rbf"
done


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
    "timestamp": $timestamp,
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