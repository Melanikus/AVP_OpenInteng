// The one way the editor's stand-ins that stop end (R-INT-15). Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng.
//
// The editor compiles most of the game, but never its network code, its film decoders' frames or its music's decoding: it never plays
// online, a film or music. If anything in the game ever reaches one of those while the editor runs, that's a wrong assumption about the
// editor, so it must show at once, never be papered over. EdgeStop says which function it was, then ends the program. It never returns,
// so a stand-in needs no value to give back. With nobody at the screen (nobody_watching.h) it writes to the error output only: a message
// box would wait for ever for someone to close it.

#ifndef AVP_OPENINTENG_EDGE_STOP_H
#define AVP_OPENINTENG_EDGE_STOP_H

__declspec(noreturn) void EdgeStop(const char *name);

#endif
