#!/usr/bin/env bash
# Run from your own computer: copies the deploy files to the game server and sets one server setting there.
#     ./deploy/push-server-env.sh                          # asks for the Discord webhook URL (input hidden)
#     ./deploy/push-server-env.sh BC_BOTS 3                # any other BC_ setting
# Needs SSH access to the server; sudo on the server asks for your password (ssh -t gives it a terminal).
# Override the target with BC_HOST=user@host.
set -euo pipefail
HOST=${BC_HOST:-john@192.168.10.181}
NAME=${1:-BC_DISCORD_WEBHOOK}
if [ $# -ge 2 ]; then
	VALUE=$2
else
	read -rsp "Value for $NAME (input hidden): " VALUE; echo
fi
[ -n "$VALUE" ] || { echo "empty value"; exit 2; }
case "$NAME" in BC_[A-Z0-9_]*) ;; *) echo "the name must look like BC_SOMETHING"; exit 2;; esac

cd "$(dirname "$0")"
REMOTE=$(ssh "$HOST" 'mktemp -d')
trap 'ssh "$HOST" "rm -rf $REMOTE" >/dev/null 2>&1 || true' EXIT
scp -q battlecity.service set-server-env.sh "$HOST:$REMOTE/"

# The value travels on stdin of the remote shell, never in a command line or process list.
printf '%s' "$VALUE" | ssh "$HOST" "cat > $REMOTE/value && chmod 600 $REMOTE/value"
ssh -t "$HOST" "cd $REMOTE && chmod +x set-server-env.sh && sudo ./set-server-env.sh '$NAME' \"\$(cat value)\""
ssh "$HOST" "journalctl -u battlecity -n 30 --no-pager | grep CBotManager | tail -2"
