#!/bin/bash
source "$(dirname "$0")/scripts/common.sh"
check_root

title "GeceDPI Turkey - Linux Kurulumu (Ana)"
info "Mod: Otomatik (--auto) - DNS=Cloudflare (önerilen)"

build_binary
install_nft_script 1
create_service \
    "--auto --queue-num 0" \
    1
configure_dns_cloudflare
start_service
print_status
