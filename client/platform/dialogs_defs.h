// Data structures describing dialogs (filled in by dialogs_generated.cpp)
#ifndef BC_DIALOGS_DEFS_H
#define BC_DIALOGS_DEFS_H

enum BCKind { BCK_LABEL, BCK_LABEL_CENTER, BCK_LABEL_RIGHT, BCK_EDIT, BCK_BUTTON, BCK_DEFBUTTON, BCK_CHECK, BCK_RADIO, BCK_LIST, BCK_COMBO };

enum BCFlags {
	BCF_MULTILINE  = 1 << 0,
	BCF_PASSWORD   = 1 << 1,
	BCF_READONLY   = 1 << 2,
	BCF_WANTRETURN = 1 << 3,
	BCF_CENTER     = 1 << 4,
	BCF_GROUP      = 1 << 5,
	BCF_SORT       = 1 << 6,
	BCF_NUMBER     = 1 << 7,
};

struct BCControlDef {
	int kind;
	int id;
	const char *text;
	int x, y, w, h;      // dialog units
	unsigned flags;
};

struct BCDialogDef {
	int id;
	const char *name;
	const char *caption;
	int w, h;            // dialog units
	bool sysmenu;
	const BCControlDef *controls;
	int controlCount;
};

extern const BCDialogDef g_dialogDefs[];
extern const int g_dialogDefCount;

#endif
