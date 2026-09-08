#!/bin/bash
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

if [[ $EUID -ne 0 ]]; then
    error "Root olarak çalıştırın: sudo bash $0"
    exit 1
fi

info "GeceDPI Linux kaldırılıyor..."

systemctl stop gecedpi.service 2>/dev/null || true
systemctl disable gecedpi.service 2>/dev/null || true

nft delete table inet gecedpi 2>/dev/null || true

rm -f /etc/systemd/system/gecedpi.service
systemctl daemon-reload

rm -f /etc/systemd/resolved.conf.d/gecedpi.conf
systemctl restart systemd-resolved

info "GeceDPI Linux başarıyla kaldırıldı."
info "Binary ve kaynak dosyalar $(dirname "$0")/../ konumunda kalmaya devam ediyor."
