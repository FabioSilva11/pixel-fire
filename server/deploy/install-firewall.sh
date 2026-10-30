#!/bin/sh
set -eu
# Only an empty pre-existing ruleset is eligible. Never flush or replace another firewall.
test "$(id -u)" = 0
test "${1:-}" = --apply
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test -z "$(nft list ruleset)" || { echo 'Existing firewall preserved; inspect it separately'; exit 2; }
ss -lntup
echo 'Expected public listeners: SSH22 and game TCP8080/27015. Verify before applying this script.'
nft -c -f "$base/firewall.nft"
install -d -m 750 /etc/zombie-zone
install -m 644 "$base/firewall.nft" /etc/zombie-zone/firewall.nft
install -m 644 "$base/zombie-zone-firewall.service" /etc/systemd/system/zombie-zone-firewall.service
systemctl daemon-reload
# A fresh SSH connection and public TLS health must succeed before cancelling this rollback timer.
systemd-run --quiet --unit=zombie-zone-firewall-rollback --on-active=2m /usr/sbin/nft delete table inet zombie_zone
systemctl enable --now zombie-zone-firewall.service
echo 'After fresh SSH and HTTPS verification: systemctl stop zombie-zone-firewall-rollback.timer'
