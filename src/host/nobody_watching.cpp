// nobody_watching.cpp: see nobody_watching.h. Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".

#include <windows.h>
#include <string.h>

#include "nobody_watching.h"

static bool nobodyWatching = false;
static HHOOK dialogGuard = NULL;
static int dialogsRefused = 0;
static char lastDialogTitle[128] = { 0 };

// Windows asks this before it makes each window on this thread (a "computer-based training" hook, which is what Windows calls the hook
// that sees windows being made). A dialog's class is always "#32770", and every message box is one. Answering 1 tells Windows not to
// make it: the call that wanted it (MessageBox, say) fails at once and goes on, with nothing on anyone's screen.
static LRESULT CALLBACK RefuseDialogs(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HCBT_CREATEWND)
	{
		char className[16] = { 0 };
		if (GetClassNameA((HWND)wParam, className, sizeof(className)) > 0 && strcmp(className, "#32770") == 0)
		{
			CBT_CREATEWNDA const *const creating = (CBT_CREATEWNDA const *)lParam;
			char const *const title = (creating && creating->lpcs && creating->lpcs->lpszName && HIWORD(creating->lpcs->lpszName))
				? creating->lpcs->lpszName : "";
			strncpy_s(lastDialogTitle, sizeof(lastDialogTitle), title, _TRUNCATE);
			dialogsRefused++;
			return 1;
		}
	}
	return CallNextHookEx(dialogGuard, code, wParam, lParam);
}

void NobodyWatching_Set()
{
	nobodyWatching = true;
	if (!dialogGuard)
	{
		// Only this thread's windows: the hook lives in this program, so it needs no file of its own
		dialogGuard = SetWindowsHookExA(WH_CBT, RefuseDialogs, NULL, GetCurrentThreadId());
	}
}

bool NobodyWatching()
{
	return nobodyWatching;
}

int NobodyWatching_DialogsRefused()
{
	// If the guard couldn't be set, nothing can be promised, so that counts as a refusal too and the run fails
	return dialogsRefused + ((nobodyWatching && !dialogGuard) ? 1 : 0);
}

const char *NobodyWatching_LastDialogTitle()
{
	return (nobodyWatching && !dialogGuard) ? "(Windows wouldn't set the guard that refuses dialogs)" : lastDialogTitle;
}
