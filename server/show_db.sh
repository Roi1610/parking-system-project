#!/bin/bash
DB="data.db"
for t in $(sqlite3 "$DB" ".tables"); do
  echo "=== $t ==="
  sqlite3 -header -column "$DB" "SELECT * FROM $t;"
  echo ""
done
