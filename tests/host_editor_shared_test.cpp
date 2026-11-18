// The test that src/host/editor_shared.h stands on its own: it is this file's first include, so this file builds only when the header includes,
// or declares ahead, every name it uses. Then a new mode file can include it as it is (11-17: placement_special_alt.cpp couldn't, and declared
// three names of its own by hand). The check is the build; the run only says it passed. tests/host_editor_shared_test.args has the editor's
// own settings (win32/VS2026/AVP_OpenInteng.vcxproj) and, unlike most tests, no /FIevery_test.h: a header read ahead of this file would hide
// a missing include, which is what this test looks for (and it has no container to check). Written 2026-11-17 by Melanikus (Bishop) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "editor_shared.h"   // FIRST, with nothing before it: this is the test

#include <stdio.h>   // for the report only. Read after the header, it can't hide anything the header lacks.

int main(int, char **)
{
	printf("  ok  editor_shared.h builds as a file's first include\n");
	printf("1 checks, 0 failed\n");
	return 0;
}
