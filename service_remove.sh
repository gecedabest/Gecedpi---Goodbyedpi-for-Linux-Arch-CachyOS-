#!/bin/bash
source "$(dirname "$0")/scripts/common.sh"
check_root

title "GeceDPI Turkey - Kaldırma"
info "Servis durduruluyor ve kaldırılıyor..."

systemctl stop gecedpi.service 2>/dev/null || true
systemctl disable gecedpi.service 2>/dev/null || true

nft delete table inet gecedpi 2>/dev/null || true

rm -f "$SERVICE_FILE"
rm -f "$NFT_SCRIPT_DST"
systemctl daemon-reload

info "DNS ayarları geri alınıyor..."
rm -f "$RESOLVED_CONF"
chattr -i "$RESOLV_CONF" 2>/dev/null || true
ln -sf /run/systemd/resolve/stub-resolv.conf "$RESOLV_CONF" 2>/dev/null || \
    echo "nameserver 1.1.1.1" > "$RESOLV_CONF"
systemctl restart systemd-resolved 2>/dev/null || true

info "GeceDPI Linux başarıyla kaldırıldı."
info "Binary ve kaynak dosyalar $INSTALL_DIR/ konumunda kalmaya devam ediyor."
