// avi_test_file.h: A SMALL AVI FILE MADE IN MEMORY, for the tests of whose code reads a video (media_parsers.h): Windows' own AVI reader takes it, and the test
// chooses its picture's format (24-bit RGB frames, or a FOURCC no decoder knows) and its sound's (16-bit PCM, or a format tag no decoder knows, or none), which
// Media Foundation can't write itself. Written 2026-11-22 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
#pragma once

#include <stdint.h>

#include <vector>

// The picture: fourcc 0 = uncompressed 24-bit RGB (BI_RGB, each frame a colour of its own); any other = that FOURCC, each frame `width * height` bytes of nothing
// any decoder reads. The sound: soundTag 0 = none; 1 = 16-bit PCM mono at 22050 Hz (a tone); any other = that WAVEFORMATEX format tag with the same bytes
std::vector<uint8_t> AviTest_Make(uint32_t fourcc, uint32_t width, uint32_t height, uint32_t frames, uint32_t fps, uint16_t soundTag);

// "XXXX" as a FOURCC number, as AVI writes it (the first letter in the lowest byte)
uint32_t AviTest_Fourcc(char const letters[4]);
