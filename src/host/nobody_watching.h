// nobody_watching.h: whether anyone is at the screen while the editor runs. Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A run that only saves a picture or proves something (editor_main.cpp's -picture, the stock proof) has nobody at the screen, and maybe
// somebody at the same machine doing something else. Then the editor must take nothing from them: its window never becomes the active
// one, never comes to the front, and NO dialog ever appears: a message box would sit on that person's screen, and the run would wait for
// someone to close it. These say so, once, and are asked where it matters (edge_stop.cpp's message box, edge_own.cpp's
// Hudson_StaysOutOfTheWay and Hudson_StopIfTheGameWantsToAsk).

#ifndef AVP_OPENINTENG_NOBODY_WATCHING_H
#define AVP_OPENINTENG_NOBODY_WATCHING_H

// From now on, nobody is at the screen. There's no way back: a run either has someone there or it doesn't. From here on, on this thread,
// Windows is told to refuse to make any dialog (a message box is one): it fails at once and is counted, instead of appearing.
void NobodyWatching_Set();

// Whether NobodyWatching_Set has been called
bool NobodyWatching();

// How many dialogs were refused since, and the title of the last one ("" if none). A run with nobody at the screen fails if any was.
int NobodyWatching_DialogsRefused();
const char *NobodyWatching_LastDialogTitle();

#endif
