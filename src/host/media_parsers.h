// media_parsers.h: WHOSE CODE PARSES A VIDEO. Media Foundation picks the code that reads a video's container (a byte-stream handler and its media source)
// and the code that decodes what is inside (a decoder), from everything registered on the PC: a codec pack's own as well as Windows'. That code then parses a
// hostile file inside the editor (Tao 3). So, before the first sample is read, every parser Media Foundation chose is traced to the module its code lives in,
// and that module must be Windows' own or Microsoft's: a valid signature (in the file, or in Windows' own catalogs, as System32's files are signed) whose signer
// is "Microsoft Windows", "Microsoft Windows Publisher" or "Microsoft Corporation". "Microsoft Windows Hardware Compatibility Publisher" is NOT one: it is
// Microsoft's stamp over a hardware maker's driver code (an Intel or NVIDIA decoder), not Microsoft's code. A folder proves nothing (System32 holds DriverStore,
// and old codec installers wrote there). Written 2026-11-22 by Melanikus (Vasquez) for AVP_OpenInteng, on Bishop 2's order (Vasquez's Video mode review, (a)), from
// the owner's PC's own registrations (AvP-docs tools/vasquez/mf/mf-survey-2026-11-22-x86.tsv). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The check asks nothing of the network: a signature is checked from what is on the PC (no revocation list is fetched).
#pragma once

#include <string>
#include <vector>

#include <windows.h>

struct IMFSourceReader;
struct IMFByteStream;
struct IMFMediaSource;
struct IMFMediaType;
struct IMFTransform;

namespace host
{
	// The signer names taken as Microsoft's own, exactly (any capitals other than these, or any other name, is not)
	bool MediaParsers_SignerIsMicrosoft(std::wstring const &signer);

	// Whether the file at `path` is signed by Microsoft (MediaParsers_SignerIsMicrosoft), its signature checked in the file or in Windows' catalogs; *signer is the
	// signer's name ("" when none), *why says what refused it
	bool MediaParsers_FileIsMicrosoft(std::wstring const &path, std::wstring *signer, std::string *why);

	// The module the code of a COM object lives in (its first pointer, the interface's table of calls, and the module that holds that table), as a full path.
	// False when that table is in no module (code made at run time, which is never taken)
	bool MediaParsers_ModuleOf(void const *object, std::wstring *path);

	// The modules loaded in this program now, each as a full path
	std::vector<std::wstring> MediaParsers_LoadedModules();

	// The modules of every parser registered on this PC, as the editor's Media Foundation (32-bit, or 64-bit) reads them: each byte-stream handler, scheme handler,
	// Media Foundation transform and DirectX media object, by its InprocServer32 (lower case, %variables% filled in). For the setup check: a module loaded while a
	// reader was set up that is one of these is a parser, and is refused unless Microsoft's; any other module is only said
	std::vector<std::wstring> MediaParsers_RegisteredModules();

	// THE CHECK, before the first ReadSample: the reader's media source and every transform on each of `streams`, each traced to its module, and every module that
	// loaded since `before` (MediaParsers_LoadedModules, taken just before the reader was made) that is a registered parser, must be Microsoft's. True when all are;
	// false with the first that isn't, in words, in *why. *said (when given) lists every module looked at, one a line, with its signer, for the log
	bool MediaParsers_CheckReader(IMFSourceReader *reader, std::vector<DWORD> const &streams, std::vector<std::wstring> const &before, std::string *why,
		std::string *said = nullptr);

	// The same check for one object the editor made itself (the resampler): its module must be Microsoft's
	bool MediaParsers_CheckObject(void const *object, char const *what, std::string *why);

	// THE CONTAINER'S READER, chosen by the editor, never by the source resolver (Bishop 2, 11-22): the byte-stream handlers registered for contentType
	// ("video/mp4"), the first whose module is Microsoft's, made and checked on its object, then asked for the media source of `stream`. Nothing of the video is
	// parsed before that module is known to be Microsoft's. *why says why not
	HRESULT MediaParsers_CreateSource(IMFByteStream *stream, wchar_t const *contentType, IMFMediaSource **source, std::string *why);

	// THE FORMATS TAKEN, by measure (AvP-docs tools/vasquez/mf/decoders-2026-11-22.txt): a picture or a sound whose native subtype is on the list has a decoder of
	// Microsoft's own on the owner's PC, or needs none. Anything else is refused (a picture) or left out (a sound), before any decoder is made
	bool MediaParsers_FormatTaken(GUID const &major, GUID const &subtype);
	bool MediaParsers_NeedsNoDecoder(GUID const &subtype);   // uncompressed: only Windows' own converter
	std::string MediaParsers_FormatWords(GUID const &subtype);   // "H.264", "Vorbis", or its FOURCC, its WAVE tag or its GUID

	// A DECODER AS MEDIA FOUNDATION LISTS IT (MFTEnumEx): its class (MFT_TRANSFORM_CLSID_Attribute; GUID_NULL for a Store package's decoder, which has none),
	// its friendly name, and its module (for one offered: the file its class is registered to, "" when it has none; for the chosen one: the file its code is in)
	struct MediaParsersDecoderName
	{
		GUID clsid = GUID_NULL;
		std::wstring friendly;
		std::wstring module;
	};

	// THE DECODER, chosen by the editor, never by Media Foundation (Bishop 2, 11-22: a codec pack's decoder of higher merit would otherwise parse the codec's
	// headers in SetInputType before any check): every decoder Windows offers for `native` (MFTEnumEx: synchronous, sorted, no hardware, no process-local ones),
	// in that order; one whose registered module isn't Microsoft's is passed over unmade; one made is kept only if its code and everything its making and its
	// SetInputType(native) loaded are Microsoft's, and is otherwise shut down. The first kept is in *decoder, its input type set. False, with the reason in
	// *why, when none is. *said (when given) gets a line for each decoder looked at; *chosen (when given) names the chosen one
	// THE DECODER IS DRIVEN BY THE EDITOR ITSELF (card 2c, Dietrich 2026-11-23, Bishop 2's order for alpha.6: "the one window left" closed): the one chosen here is
	// fed the stream's compressed samples by video_import.cpp (ProcessInput / ProcessOutput, then Windows' own Video Processor MFT or Audio Resampler), and the
	// source reader reads each stream at its own native type, so it makes no decoder at all and Media Foundation never picks one. (Before, the chosen decoder was
	// registered for this process for the reader to make, and had the reader's SetInputType on it ever failed, Media Foundation would have gone on to the next
	// decoder on the PC, whose SetInputType would have read the codec's headers before the check refused it.)
	bool MediaParsers_ChooseDecoder(IMFMediaType *native, IMFTransform **decoder, std::string *why, std::string *said = nullptr, MediaParsersDecoderName *chosen = nullptr);
}
