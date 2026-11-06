#!/usr/bin/env bash
# Run from your own computer: links an existing character to your Outiva account on the game server.
#     ./deploy/link-outiva-account.sh Skeletor Skeletor2     (old character, the character your Outiva account has now)
# Copies deploy/link_outiva_account.py to the server and runs it with sudo (asks for your sudo password), with the game service
# stopped for those few seconds so nobody is saved over the change (players are disconnected briefly). BC_HOST overrides the target.
set -euo pipefail
HOST=${BC_HOST:-john@192.168.10.181}
[ $# -eq 2 ] || { echo "usage: $0 <old-character> <outiva-character>"; exit 2; }
cd "$(dirname "$0")"
REMOTE=$(ssh "$HOST" 'mktemp -d')
trap 'ssh "$HOST" "rm -rf $REMOTE" >/dev/null 2>&1 || true' EXIT
scp -q link_outiva_account.py "$HOST:$REMOTE/"
ssh -t "$HOST" "sudo systemctl stop battlecity && { sudo python3 $REMOTE/link_outiva_account.py '$1' '$2'; rc=\$?; sudo systemctl start battlecity; exit \$rc; }"
