// dll_pins.h: the SHA-256 of each LGPL DLL the editor loads (Barry's libsmackerdec.dll for the FMV preview, Frost's libsmackerenc.dll for Import video), as the build was given them.
// Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng, with Vasquez (Import video). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// BISHOP'S RULING (11-18, for both DLLs): the pin is the default, so only the DLL shipped beside the editor loads; its hash is taken from the SHIPPED file (release.yml hashes each DLL
// going into the zip and passes it to the editor's MSBuild as the property SmackerDecoderSha256 / SmackerEncoderSha256; AVP_OpenInteng.vcxproj turns them into the defines below,
// the bare 64 hex digits). A build given none pins none ("": nothing loads unless the user's AVP_OpenInteng.ini names a hash, LGPL 2.1 section 6: the user may put his own build of
// the library in its place; the status line then says which file loaded and why). The hex is passed bare and made a string here, because a quoted string does not survive MSBuild's
// /D reliably; the .vcxproj puts an x before it (x<hex>, or x alone for none), so the define is never empty (an empty one is MSVC's warning C4003) and the string starts one letter on.
#pragma once

#define DLL_PIN_STRING_(x) #x
#define DLL_PIN_STRING(x) DLL_PIN_STRING_(x)

#ifndef SMACKER_DECODER_SHA256
#define SMACKER_DECODER_SHA256 x
#endif

#ifndef SMACKER_ENCODER_SHA256
#define SMACKER_ENCODER_SHA256 x
#endif

namespace host
{
	// 64 hex digits, or "" when the build was given none
	char const *const kDllPinSmackerDecoder = DLL_PIN_STRING(SMACKER_DECODER_SHA256) + 1;   // past the x
	char const *const kDllPinSmackerEncoder = DLL_PIN_STRING(SMACKER_ENCODER_SHA256) + 1;
}
