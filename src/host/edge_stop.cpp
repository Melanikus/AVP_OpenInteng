// The editor's stop for a stand-in that was reached (see edge_stop.h). Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "edge_stop.h"
#include "nobody_watching.h"

void EdgeStop(const char *name)
{
	// The name is one of the stand-ins' own, written in this program; it's still shown as text, never as a format.
	char message[512];
	_snprintf_s(message, sizeof(message), _TRUNCATE,
		"AVP_OpenInteng reached a part of the game it never runs (%s). It stops here, so nothing goes on from a wrong place. Please report "
		"this message.", name ? name : "?");

	fprintf(stderr, "%s\n", message);
	if (!NobodyWatching())
	{
		MessageBoxA(NULL, message, "AVP_OpenInteng", MB_OK | MB_ICONERROR);
	}
	ExitProcess(3);
}
