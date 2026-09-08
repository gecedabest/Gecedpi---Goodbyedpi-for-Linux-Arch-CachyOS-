#!/bin/bash
source "$(dirname "$0")/scripts/common.sh"
check_root

title "GeceDPI Turkey - Alternatif 4 SuperOnline (Fragment=5, DNS=Cloudflare)"
info "Mod: Fragment=5 byte, Fake=Kapalı, DNS=Cloudflare"

build_binary
install_nft_script 1
create_service \
    "--fragment-size 5 --no-fake --dns-addr 1.1.1.1 --dns-port 53 --dnsv6-addr 2606:4700:4700::1111 --dnsv6-port 53 --queue-num 0" \
    1
configure_dns_cloudflare
start_service
print_status
