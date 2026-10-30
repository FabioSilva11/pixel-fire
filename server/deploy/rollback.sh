#!/bin/sh
set -eu
test "$(id -u)" = 0
test -n "${1:-}"
backup=$(realpath "$1")
case "$backup" in /var/backups/zombie-zone/deploy-*) ;; *) echo 'Invalid game rollback snapshot'; exit 2;; esac
test -f "$backup/zombie-zone.service"
cp -p "$backup/zombie-zone.service" /etc/systemd/system/zombie-zone.service
if [ -f "$backup/battle-royale.env" ]; then cp -p "$backup/battle-royale.env" /etc/zombie-zone/battle-royale.env; fi
systemctl daemon-reload
systemctl restart zombie-zone.service
systemctl --no-pager --full status zombie-zone.service
