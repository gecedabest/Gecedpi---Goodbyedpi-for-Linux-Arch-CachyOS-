#!/bin/bash
INSTALL_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$INSTALL_DIR/scripts/common.sh"
check_root

title "GeceDPI Turkey - Tek Seferlik Çalıştırma"
info "Mod: Otomatik (--auto) - DNS=Cloudflare (önerilen)"
warn "Bu terminal kapatılırsa gecedpi durur!"
warn "Sistem servisi için: sudo bash service_install_dnsredir_turkey.sh"
echo ""

build_binary
install_nft_script 1

DNS_REDIR=1 "$NFT_SCRIPT_DST"

configure_dns_cloudflare

info "gecedpi başlatılıyor (Ctrl+C ile durdur)..."
"$BINARY" --auto --queue-num 0

info "Temizleniyor..."
/usr/sbin/nft delete table inet gecedpi 2>/dev/null
info "GeceDPI durduruldu."
