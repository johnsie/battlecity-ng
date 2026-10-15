#!/usr/bin/env bash
# Runs as the service user (ExecStartPre), working directory = state dir (/var/lib/battlecity).
# 1. Seeds map.dat on first start.  2. Backs up the database (at most once an hour, newest 14 kept).
set -eu
RELEASE=$(dirname "$(readlink -f "$0")")

[ -f map.dat ] || cp "$RELEASE/map.dat" map.dat

if [ -f dat/BCData.db ]; then
  mkdir -p backups
  newest=$(ls -1t backups/BCData-*.db 2>/dev/null | head -n 1 || true)
  if [ -z "$newest" ] || [ -n "$(find "$newest" -mmin +60)" ]; then
    cp dat/BCData.db "backups/BCData-$(date +%Y%m%d-%H%M%S).db"
    ls -1t backups/BCData-*.db | tail -n +15 | xargs -r rm -f
  fi
fi
