#!/usr/bin/env python3
"""Moves an Outiva link from one character to another (run as root ON the game server, with the service stopped).
    link_outiva_account.py <old-character> <outiva-character>      e.g.  Skeletor Skeletor2
The Outiva account's id (OutivaSub) is moved from the second character to the first, so signing in from Outiva plays the old
character with its points, rank and tanks. The second character is kept (renamed <name>_old, unlinked), never deleted.
The database is backed up first. Shows what it will do and asks before changing anything."""
import shutil, sqlite3, sys, time

DB = "/var/lib/battlecity/dat/BCData.db"
if len(sys.argv) != 3:
    sys.exit(__doc__)
old, new = sys.argv[1], sys.argv[2]

con = sqlite3.connect(DB)
def row(name):
    return con.execute("SELECT AccountID, Account, Points, OutivaSub, Guest FROM tAccounts WHERE Account = ? COLLATE NOCASE", (name,)).fetchone()
a, b = row(old), row(new)
for name, r in ((old, a), (new, b)):
    if r is None:
        sys.exit("no character called %r" % name)
    print("%-16s id=%-5s points=%-8s linked to Outiva: %s%s" % (r[1], r[0], r[2], "yes" if r[3] else "no", "  (GUEST)" if r[4] else ""))
if a[0] == b[0]: sys.exit("same character")
if a[3]: sys.exit("%s is already linked to an Outiva account" % a[1])
if not b[3]: sys.exit("%s is not linked to an Outiva account, so there is nothing to move" % b[1])
if a[4] or b[4]: sys.exit("a guest row is involved")
retired = (b[1] + "_old")[:15]
if row(retired): sys.exit("%s already exists" % retired)
print("\nThe Outiva account moves from %s to %s; %s becomes %s (kept, no longer linked)." % (b[1], a[1], b[1], retired))
if input("Type yes to do it: ").strip().lower() != "yes":
    sys.exit("cancelled")
backup = "%s.before-link-%d" % (DB, time.time())
shutil.copy2(DB, backup)
print("backup:", backup)
with con:
    con.execute("UPDATE tAccounts SET OutivaSub = '' , Account = ? WHERE AccountID = ?", (retired, b[0]))
    con.execute("UPDATE tAccounts SET OutivaSub = ? WHERE AccountID = ?", (b[3], a[0]))
print("done: signing in from Outiva now plays %s" % a[1])
