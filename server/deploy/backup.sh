#!/bin/sh
set -eu
# Public identity fingerprints/results only. TLS/SSH secrets are never copied into source releases.
test "$(id -u)" = 0
dest=${ZZ_BACKUP_DIR:-/var/backups/zombie-zone}
install -d -m 700 "$dest"
stamp=$(date -u +%Y%m%dT%H%M%SZ)
tar -C /var/lib/zombie-zone -czf "$dest/data-$stamp.tar.gz" .
chmod 600 "$dest/data-$stamp.tar.gz"
