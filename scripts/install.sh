#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
check_root

title "GeceDPI Turkey - Kurulum"
build_binary
install_nft_script 1
create_service "--auto --queue-num 0" 1
configure_dns_cloudflare
start_service
print_status