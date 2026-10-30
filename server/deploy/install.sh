#!/bin/sh
set -eu
# Explicit, reviewable release activation. Existing service/data/certificates get a recoverable snapshot.
test "$(id -u)" = 0
if [ "${1:-}" != --apply ] || [ -z "${2:-}" ]; then
    echo 'Usage: install.sh --apply /opt/zombie-zone/releases/RELEASE'
    exit 2
fi
release=$(realpath "$2")
case "$release" in /opt/zombie-zone/releases/*) ;; *) echo 'Release outside game directory'; exit 2;; esac
test -x /opt/zombie-zone/node-v24.22.1-linux-x64/bin/node
test -f "$release/server/native/build/zzbr.node"
test -f "$release/terra-escarlate.zzmap"
test -f "$release/map-manifest.json"
test -f /etc/zombie-zone/server.crt
test -f /etc/zombie-zone/server.key
test -f "$release/server/deploy/zombie-zone.service"
getent passwd zombie-zone >/dev/null
install -d -m 700 /var/backups/zombie-zone
backup=/var/backups/zombie-zone/deploy-$(date -u +%Y%m%dT%H%M%SZ)
install -d -m 700 "$backup"
cp -p /etc/systemd/system/zombie-zone.service "$backup/zombie-zone.service"
if [ -f /etc/zombie-zone/battle-royale.env ]; then cp -p /etc/zombie-zone/battle-royale.env "$backup/battle-royale.env"; fi
tar -C /var/lib/zombie-zone -czf "$backup/persistent-data.tar.gz" .
install -d -m 750 -o zombie-zone -g zombie-zone /var/log/zombie-zone /var/lib/zombie-zone
install -d -m 750 -o root -g zombie-zone /etc/zombie-zone
install -d -m 755 /usr/local/lib/zombie-zone
install -m 755 "$release/server/deploy/backup.sh" /usr/local/lib/zombie-zone/backup.sh
install -m 644 "$release/server/deploy/zombie-zone.logrotate" /etc/logrotate.d/zombie-zone
install -m 644 "$release/server/deploy/zombie-zone-backup.service" /etc/systemd/system/zombie-zone-backup.service
install -m 644 "$release/server/deploy/zombie-zone-backup.timer" /etc/systemd/system/zombie-zone-backup.timer
cat > /etc/zombie-zone/battle-royale.env <<EOF
ZZ_MAP_FILE=$release/terra-escarlate.zzmap
ZZ_MAX_PLAYERS=8
ZZ_API_PORT=8080
ZZ_GAME_PORT=27015
ZZ_DATA_FILE=/var/lib/zombie-zone/results-v3.json
EOF
chmod 640 /etc/zombie-zone/battle-royale.env
chown root:zombie-zone /etc/zombie-zone/battle-royale.env
sed -e "s|WorkingDirectory=.*|WorkingDirectory=$release/server|" -e 's|EnvironmentFile=.*|EnvironmentFile=-/etc/zombie-zone/battle-royale.env|' "$release/server/deploy/zombie-zone.service" > /etc/systemd/system/zombie-zone.service
systemctl daemon-reload
systemctl enable zombie-zone.service zombie-zone-backup.timer
if ! systemctl restart zombie-zone.service; then
    sh "$release/server/deploy/rollback.sh" "$backup"
    exit 1
fi
ok=false
for attempt in 1 2 3 4 5; do
    if curl --fail --silent --cacert /etc/zombie-zone/server.crt --resolve 187.102.244.208:8080:127.0.0.1 https://187.102.244.208:8080/health > "$backup/new-health.json" && /opt/zombie-zone/node-v24.22.1-linux-x64/bin/node -e 'const fs=require("node:fs"),h=JSON.parse(fs.readFileSync(process.argv[1])),m=JSON.parse(fs.readFileSync(process.argv[2]));if(!h.ok||h.protocol!==1||h.mapVersion!==4||h.mapHash!==m.hash)process.exit(1)' "$backup/new-health.json" "$release/map-manifest.json"; then ok=true; break; fi
    sleep 1
done
if [ "$ok" != true ]; then
    echo 'Health failed; restoring previous service'
    cp -p "$backup/zombie-zone.service" /etc/systemd/system/zombie-zone.service
    if [ -f "$backup/battle-royale.env" ]; then cp -p "$backup/battle-royale.env" /etc/zombie-zone/battle-royale.env; fi
    systemctl daemon-reload
    systemctl restart zombie-zone.service
    exit 1
fi
systemctl start zombie-zone-backup.timer
printf 'Activated release: %s\nRollback snapshot: %s\n' "$release" "$backup"
cat "$backup/new-health.json"
