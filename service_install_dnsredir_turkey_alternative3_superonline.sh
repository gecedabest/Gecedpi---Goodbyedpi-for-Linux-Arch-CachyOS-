#!/bin/bash
source "$(dirname "$0")/scripts/common.sh"
check_root

title "GeceDPI Turkey - Alternatif 3 SuperOnline (TTL=3, DNS=Cloudflare)"
info "Mod: Fake TTL=3, Fragment=Kapalı, DNS=Cloudflare"

build_binary
install_nft_script 1
create_service \
    "--no-fragment --set-ttl 3 --dns-addr 1.1.1.1 --dns-port 53 --dnsv6-addr 2606:4700:4700::1111 --dnsv6-port 53 --queue-num 0" \
    1
configure_dns_cloudflare
start_service
print_status
