#!/usr/bin/env python3
"""Removes the legacy bc.outiva.com <VirtualHost> blocks from an Apache site file.

A block is removed only if it names bc.outiva.com AND every name it serves (ServerName / ServerAlias)
is one of the legacy names below. A block that serves anything else is never touched; the script
then stops with an error instead.

Usage: remove-legacy-vhost.py <site-file>      (prints what it did; exit 0 = ok or nothing to do)
"""
import re, sys

LEGACY_NAMES = {"bc.outiva.com", "battlecity.orb", "www.battlecity.orb"}
BLOCK = re.compile(r"<VirtualHost\b[^>]*>.*?</VirtualHost>[ \t]*\n?", re.S | re.I)

def names_of(block):
    names = set()
    for m in re.finditer(r"^\s*Server(?:Name|Alias)\s+(.+)$", block, re.M | re.I):
        names.update(m.group(1).split())
    return {n.lower() for n in names}

path = sys.argv[1]
text = open(path, encoding="utf-8", errors="surrogateescape").read()
removed = 0

def drop(match):
    global removed
    block = match.group(0)
    names = names_of(block)
    if "bc.outiva.com" not in names:
        return block
    extra = names - LEGACY_NAMES
    if extra:
        sys.exit("refusing to remove a bc.outiva.com vhost that also serves %s" % sorted(extra))
    removed += 1
    return ""

new = BLOCK.sub(drop, text)
if removed == 0:
    print("no legacy bc.outiva.com vhost in", path)
    sys.exit(0)
new = re.sub(r"\n{4,}", "\n\n\n", new).rstrip() + "\n"   # tidy the gap the removed blocks leave
open(path, "w", encoding="utf-8", errors="surrogateescape").write(new)
print("removed %d legacy vhost block(s) from %s" % (removed, path))
