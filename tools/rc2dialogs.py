#!/usr/bin/env python3
"""Generates client/platform/dialogs_generated.cpp from client/BattleCityClient.rc.

The Win32 resource script describes every dialog (size, caption, controls). We turn it into C++ tables
that platform/dialogs.cpp renders with Dear ImGui. Re-run after editing the .rc:
    python3 tools/rc2dialogs.py
"""
import re, sys, os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RC = os.path.join(ROOT, "client", "BattleCityClient.rc")
RES = os.path.join(ROOT, "client", "resource.h")
OUT = os.path.join(ROOT, "client", "platform", "dialogs_generated.cpp")

# --- symbol table from resource.h ---------------------------------------------------------
symbols = {"IDC_STATIC": -1, "IDOK": 1, "IDCANCEL": 2}
for line in open(RES, encoding="latin-1"):
    m = re.match(r"#define\s+(\w+)\s+(-?\d+)", line)
    if m:
        symbols[m.group(1)] = int(m.group(2))

def num(token):
    token = token.strip()
    if re.fullmatch(r"-?\d+", token):
        return int(token)
    if token in symbols:
        return symbols[token]
    raise SystemExit("unknown symbol in .rc: %r" % token)

# --- style flags we care about ------------------------------------------------------------
FLAGS = {
    "ES_MULTILINE": "BCF_MULTILINE", "ES_PASSWORD": "BCF_PASSWORD", "ES_READONLY": "BCF_READONLY",
    "ES_WANTRETURN": "BCF_WANTRETURN", "ES_CENTER": "BCF_CENTER", "WS_GROUP": "BCF_GROUP",
    "CBS_SORT": "BCF_SORT", "ES_NUMBER": "BCF_NUMBER",
}
KINDS = {"LTEXT": "BCK_LABEL", "CTEXT": "BCK_LABEL_CENTER", "RTEXT": "BCK_LABEL_RIGHT", "EDITTEXT": "BCK_EDIT",
         "PUSHBUTTON": "BCK_BUTTON", "DEFPUSHBUTTON": "BCK_DEFBUTTON", "LISTBOX": "BCK_LIST", "COMBOBOX": "BCK_COMBO"}

def flags_of(style):
    out = [FLAGS[f] for f in re.findall(r"[A-Z_]+", style or "") if f in FLAGS]
    return " | ".join(out) if out else "0"

def split_args(s):
    """split on commas that are not inside quotes"""
    parts, cur, inq, i = [], "", False, 0
    while i < len(s):
        ch = s[i]
        if ch == '"':
            if inq and i + 1 < len(s) and s[i + 1] == '"':
                cur += '""'; i += 2; continue
            inq = not inq
        if ch == "," and not inq:
            parts.append(cur.strip()); cur = ""
        else:
            cur += ch
        i += 1
    parts.append(cur.strip())
    return parts

def unquote(s):
    s = s.strip()
    if s.startswith('"') and s.endswith('"'):
        s = s[1:-1].replace('""', '"')
        # RC strings use C-style escapes
        s = re.sub(r"\\(n|t|r|\\)", lambda m: {"n": "\n", "t": "\t", "r": "", "\\": "\\"}[m.group(1)], s)
    return s

def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "") + '"'

# --- parse ---------------------------------------------------------------------------------
text = open(RC, encoding="latin-1").read().replace("\r\n", "\n")
# join continuation lines (a control spilling over onto the next line ends its first line with a comma)
lines, buf = [], ""
for raw in text.split("\n"):
    if buf:
        buf += " " + raw.strip()
    else:
        buf = raw
    if buf.rstrip().endswith(","):
        continue
    lines.append(buf)
    buf = ""

dialogs = []
i = 0
while i < len(lines):
    m = re.match(r"^(\w+)\s+DIALOGEX\s+(\d+),\s*(\d+),\s*(\d+),\s*(\d+)", lines[i])
    if not m:
        i += 1
        continue
    name, w, h = m.group(1), int(m.group(4)), int(m.group(5))
    dlg = {"name": name, "id": num(name), "w": w, "h": h, "caption": "", "sysmenu": False, "controls": []}
    i += 1
    while not lines[i].strip().startswith("BEGIN"):
        s = lines[i].strip()
        if s.startswith("CAPTION"):
            dlg["caption"] = unquote(s[len("CAPTION"):])
        if s.startswith("STYLE") and "WS_SYSMENU" in s:
            dlg["sysmenu"] = True
        i += 1
    i += 1
    while not lines[i].strip().startswith("END"):
        s = lines[i].strip()
        i += 1
        if not s:
            continue
        kind, _, rest = s.partition(" ")
        rest = rest.strip()
        a = split_args(rest)
        if kind in ("LTEXT", "CTEXT", "RTEXT", "PUSHBUTTON", "DEFPUSHBUTTON"):
            txt, cid, x, y, cw, ch = unquote(a[0]), num(a[1]), *map(int, a[2:6])
            style = a[6] if len(a) > 6 else ""
        elif kind in ("EDITTEXT", "LISTBOX", "COMBOBOX"):
            txt, cid, x, y, cw, ch = "", num(a[0]), *map(int, a[1:5])
            style = a[5] if len(a) > 5 else ""
        elif kind == "CONTROL":
            txt, cid, cls, style = unquote(a[0]), num(a[1]), unquote(a[2]), a[3]
            x, y, cw, ch = map(int, a[4:8])
            if "BS_AUTORADIOBUTTON" in style:
                k = "BCK_RADIO"
            elif "BS_AUTOCHECKBOX" in style:
                k = "BCK_CHECK"
            else:
                raise SystemExit("unsupported CONTROL style: " + s)
            dlg["controls"].append((k, cid, txt, x, y, cw, ch, flags_of(style)))
            continue
        else:
            raise SystemExit("unsupported control: " + s)
        dlg["controls"].append((KINDS[kind], cid, txt, x, y, cw, ch, flags_of(style)))
    dialogs.append(dlg)

# --- emit ----------------------------------------------------------------------------------
out = ['// GENERATED by tools/rc2dialogs.py from client/BattleCityClient.rc - do not edit by hand.',
       '#include "dialogs_defs.h"', '']
for d in dialogs:
    out.append("static const BCControlDef controls_%s[] = {" % d["name"])
    for (k, cid, txt, x, y, w, h, fl) in d["controls"]:
        out.append("    { %s, %d, %s, %d, %d, %d, %d, %s }," % (k, cid, cstr(txt), x, y, w, h, fl))
    out.append("};")
out.append("")
out.append("const BCDialogDef g_dialogDefs[] = {")
for d in dialogs:
    out.append("    { %d, %s, %s, %d, %d, %s, controls_%s, %d }," % (
        d["id"], cstr(d["name"]), cstr(d["caption"]), d["w"], d["h"], "true" if d["sysmenu"] else "false", d["name"], len(d["controls"])))
out.append("};")
out.append("const int g_dialogDefCount = %d;" % len(dialogs))
open(OUT, "w", newline="\n").write("\n".join(out) + "\n")
print("wrote", os.path.relpath(OUT, ROOT), "-", len(dialogs), "dialogs,", sum(len(d["controls"]) for d in dialogs), "controls")
