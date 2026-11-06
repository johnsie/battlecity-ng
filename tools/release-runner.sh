#!/usr/bin/env bash
# Frees the GitLab runner on the server when an old pipeline is stuck holding it.
#
#   tools/release-runner.sh          show what the runner is doing, then ask before restarting it
#   tools/release-runner.sh -y       restart without asking
#   tools/release-runner.sh -l       only show (list), change nothing
#
# Logs in as john on 192.168.10.181 (override with RUNNER_HOST / RUNNER_USER). Restarting gitlab-runner
# needs sudo, so ssh gets a terminal and sudo asks for john's password. Restarting stops the running job
# (GitLab marks it failed) and the runner picks up the next one. Cancel the pipeline in GitLab too, or it
# may be retried.
set -euo pipefail

HOST="${RUNNER_HOST:-192.168.10.181}"
USER_="${RUNNER_USER:-john}"
mode=ask
case "${1:-}" in
  -y) mode=yes ;;
  -l) mode=list ;;
  "") ;;
  *) echo "usage: $0 [-y|-l]" >&2; exit 2 ;;
esac

echo "== runner state on $USER_@$HOST"
ssh -t "$USER_@$HOST" '
  echo "-- gitlab-runner service:"; systemctl is-active gitlab-runner || true
  echo "-- job processes (with run time):"
  ps -eo pid,etime,user,cmd --sort=start_time | grep -E "gitlab-runner|builds/|ci/(deploy|prestart|smoke|bots|relay|load)" | grep -v grep || echo "(none)"
'

[ "$mode" = list ] && exit 0

if [ "$mode" = ask ]; then
  read -r -p "Restart gitlab-runner on $HOST to release it? [y/N] " a
  [ "$a" = y ] || [ "$a" = Y ] || { echo "left alone"; exit 0; }
fi

echo "== restarting (sudo will ask for the password)"
ssh -t "$USER_@$HOST" '
  sudo systemctl restart gitlab-runner &&
  sleep 2 &&
  echo "-- now:" && systemctl is-active gitlab-runner &&
  (sudo gitlab-runner verify 2>&1 | tail -5 || true)
'
echo "Done. The runner is free; cancel the stuck pipeline in GitLab if it still shows as running."
