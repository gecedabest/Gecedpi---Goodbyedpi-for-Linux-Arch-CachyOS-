#!/bin/bash
source "$(dirname "$0")/scripts/common.sh"
check_root

title "GeceDPI Turkey - Alternatif 1 SuperOnline (TTL=3, DNS manuel)"
warn "Bu mod DNS yönlendirmesi yapmaz!"
warn "DNS'i manuel olarak 1.1.1.1 veya 1.0.0.1 yapın."
echo ""

build_binary
install_nft_script 0
create_service \
    "--no-fragment --set-ttl 3 --queue-num 0" \
    0
start_service
print_status
