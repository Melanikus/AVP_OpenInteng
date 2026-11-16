// crash_guard.h: runs a piece of a test so that a crash (an access violation) is reported as a check that FAILED, with
// that check's own words, and the test goes on. Written 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// WHY. A hostile-input test has to name its reason (CODING-STANDARD.md, rule 3). When the code under test loses a guard (a null
// pointer's check, a bound), the next thing it does can be to read or write through a bad pointer, and the whole program stops: the
// take-out row that removed the guard is then "caught", but only by the stop, which says nothing about what was wrong. Run under
// Survives, the same crash makes THAT check FAILED, by name, and the other checks still run.
//
// HOW TO USE. A check that could crash if its guard were gone:
//     bool refused = false;
//     Check(Survives([&] { refused = !ReadFieldAxis(kind, field, 0, length, 0, &value, &why); }) && refused, "no payload is refused");
// Survives gives back false if the code inside crashed (an access violation, or another exception the system raises), true if it ran to its end. What the code under test does must
// not need anything undone after a crash: a crash here means the test has already FAILED.
//
// ONLY FOR WINDOWS (the editor's tests are built with Microsoft's compiler, which has the structured exception handling this uses).

#pragma once

// windows.h gives the structured exception handling this uses; without these two it also defines min and max as macros, which break the
// standard library's own, and drags in a great deal the tests never use
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace crash_guard
{
	// A function with no C++ objects in it, because Microsoft's compiler won't put __try in one that has (error C2712)
	inline bool RunUnderGuard(void (*body)(void *), void *context)
	{
		__try
		{
			body(context);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}
}

// Runs body (a lambda or any callable with no arguments); false if it crashed
template <class Body>
bool Survives(Body const &body)
{
	return crash_guard::RunUnderGuard([](void *context) { (*static_cast<Body const *>(context))(); }, (void *)&body);
}
