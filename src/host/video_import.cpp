// See video_import.h. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "video_import.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <wmcodecdsp.h>
#include <wrl/client.h>

namespace host
{
	namespace
	{
		// Windows' device names, which name a device in any folder and with any ending (CON.mp4 is the console): CON PRN AUX NUL, COM0-9 and LPT0-9 (and their superscript
		// 1 2 3 forms), CONIN$ CONOUT$
		bool IsDeviceName(std::wstring const &leaf)
		{
			// the name up to its first dot, spaces at its end dropped (Windows drops them too: "lpt9 .wmv" is LPT9)
			std::wstring base = leaf.substr(0, leaf.find(L'.'));

			while (!base.empty() && (base.back() == L' ' || base.back() == L'.'))
			{
				base.pop_back();
			}

			for (wchar_t &letter : base)
			{
				letter = (wchar_t)towupper(letter);
			}

			static wchar_t const *const kPlain[] = { L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$" };

			for (wchar_t const *name : kPlain)
			{
				if (base == name)
				{
					return true;
				}
			}

			if (base.size() == 4 && (base.compare(0, 3, L"COM") == 0 || base.compare(0, 3, L"LPT") == 0))
			{
				wchar_t const last = base[3];

				return (last >= L'0' && last <= L'9') || last == 0x00b9 || last == 0x00b2 || last == 0x00b3;
			}

			return false;
		}

		std::string Narrow(std::wstring const &wide)
		{
			std::string narrow;

			for (wchar_t const letter : wide)
			{
				narrow.push_back(letter >= 32 && letter < 127 ? (char)letter : '?');   // the status line's letters only
			}

			return narrow;
		}

		VideoImportResult Refuse(VideoImportResult result, std::string const &words, std::string *why)
		{
			if (why)
			{
				*why = words;
			}

			return result;
		}
	}

	char const *VideoImport_Words(VideoImportResult result)
	{
		switch (result)
		{
		case VideoImportResult::Done: return "done";
		case VideoImportResult::BadCall: return "bad call";
		case VideoImportResult::NotAFile: return "not a plain file";
		case VideoImportResult::NotAVideoName: return "not a video's name (.mp4, .m4v, .avi or .wmv)";
		case VideoImportResult::Empty: return "the file is empty";
		case VideoImportResult::TooBig: return "the file is too big";
		case VideoImportResult::CantRead: return "Windows couldn't read the file";
		case VideoImportResult::TooLong: return "the video is too long";
		case VideoImportResult::TooLarge: return "the video's picture is too large";
		case VideoImportResult::NotAVideo: return "there's no picture in it";
		case VideoImportResult::BadPicture: return "its picture can't be decoded";
		case VideoImportResult::BadSound: return "its sound can't be decoded";
		case VideoImportResult::NoMediaFoundation: return "Windows' Media Foundation isn't there";
		}

		return "unknown";
	}

	wchar_t const *VideoImport_ContentType(std::wstring const &path)
	{
		size_t const slash = path.find_last_of(L"\\/");
		size_t const dot = path.find_last_of(L'.');

		if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
		{
			return nullptr;
		}

		std::wstring ending = path.substr(dot);

		for (wchar_t &letter : ending)
		{
			letter = (wchar_t)towlower(letter);
		}

		if (ending == L".mp4" || ending == L".m4v")
		{
			return L"video/mp4";
		}

		if (ending == L".avi")
		{
			return L"video/avi";
		}

		if (ending == L".wmv")
		{
			return L"video/x-ms-wmv";
		}

		return nullptr;
	}

	VideoImportResult VideoImport_ReadFile(std::wstring const &path, std::vector<uint8_t> *bytes, std::string *why)
	{
		if (!bytes)
		{
			return Refuse(VideoImportResult::BadCall, "no place for the file's bytes", why);
		}

		if (!VideoImport_ContentType(path))
		{
			return Refuse(VideoImportResult::NotAVideoName, "Import video: a video's name ends in .mp4, .m4v, .avi or .wmv", why);
		}

		// a 0 inside the name would end it early and name another file
		if (path.find(L'\0') != std::wstring::npos)
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: the name has a 0 in it", why);
		}

		// the whole path, as Windows reads it; never a device path (\\.\) or a path that skips Windows' own reading of it (\\?\)
		DWORD const need = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);

		if (need == 0 || need > 32768)
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: Windows can't make a whole path of that name", why);
		}

		std::wstring full(need, L'\0');
		DWORD const got = GetFullPathNameW(path.c_str(), need, &full[0], nullptr);

		if (got == 0 || got >= need)
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: Windows can't make a whole path of that name", why);
		}

		full.resize(got);

		if (full.compare(0, 4, L"\\\\.\\") == 0 || full.compare(0, 4, L"\\\\?\\") == 0 || full.compare(0, 4, L"\\??\\") == 0)
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: " + Narrow(full) + " is a device's or a raw path, not a plain file's", why);
		}

		// a file's own stream (name:stream): the only colon in a plain path is the drive's
		if (full.find(L':', 2) != std::wstring::npos)
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: " + Narrow(full) + " names a file's own stream, not a plain file", why);
		}

		size_t const slash = full.find_last_of(L'\\');
		std::wstring const leaf = slash == std::wstring::npos ? full : full.substr(slash + 1);

		if (IsDeviceName(leaf))
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: " + Narrow(leaf) + " is a device's name in Windows, not a file's", why);
		}

		// opened once, the link itself if it is one (never followed), shared for reading only; everything below is judged on this handle
		HANDLE const file = CreateFileW(full.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			{
				return Refuse(VideoImportResult::NotAFile, "Import video: there's no file " + Narrow(full), why);
			}

			return Refuse(error == ERROR_ACCESS_DENIED ? VideoImportResult::NotAFile : VideoImportResult::CantRead, "Import video: Windows won't open " + Narrow(full) + " (error " + std::to_string((unsigned long)error) + ")", why);
		}

		struct Closer
		{
			HANDLE handle;
			~Closer() { CloseHandle(handle); }
		} const closer = { file };

		FILE_ATTRIBUTE_TAG_INFO tag = {};

		if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandleEx(file, FileAttributeTagInfo, &tag, sizeof(tag)))
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: " + Narrow(full) + " is not a file on a disk", why);
		}

		if (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: " + Narrow(full) + " is a link (a shortcut Windows follows), not the video itself", why);
		}

		if (tag.FileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE))
		{
			return Refuse(VideoImportResult::NotAFile, "Import video: " + Narrow(full) + " is a folder, not a file", why);
		}

		LARGE_INTEGER size = {};

		if (!GetFileSizeEx(file, &size) || size.QuadPart < 0)
		{
			return Refuse(VideoImportResult::CantRead, "Import video: Windows won't tell the size of " + Narrow(full), why);
		}

		if (size.QuadPart == 0)
		{
			return Refuse(VideoImportResult::Empty, "Import video: " + Narrow(full) + " is empty", why);
		}

		if ((uint64_t)size.QuadPart > kVideoImportMostBytes)
		{
			return Refuse(VideoImportResult::TooBig, "Import video: " + Narrow(leaf) + " is " + std::to_string((unsigned long long)(size.QuadPart >> 20)) + " MB; at most " +
				std::to_string((unsigned long long)(kVideoImportMostBytes >> 20)) + " MB is taken", why);
		}

		// read whole: no more than the size said (a file growing meanwhile is cut there), and all of it
		std::vector<uint8_t> read((size_t)size.QuadPart);
		size_t done = 0;

		while (done < read.size())
		{
			DWORD const ask = (DWORD)(read.size() - done > (1u << 24) ? (1u << 24) : read.size() - done);
			DWORD part = 0;

			if (!ReadFile(file, read.data() + done, ask, &part, nullptr) || part == 0)
			{
				return Refuse(VideoImportResult::CantRead, "Import video: Windows stopped reading " + Narrow(leaf) + " after " + std::to_string((unsigned long long)done) + " bytes", why);
			}

			done += part;
		}

		bytes->swap(read);

		if (why)
		{
			why->clear();
		}

		return VideoImportResult::Done;
	}

	VideoImportResult VideoImport_CheckStream(uint32_t width, uint32_t height, int64_t length, std::string *why, int64_t mostSeconds)
	{
		if (mostSeconds < 1 || mostSeconds > kVideoImportLongestSeconds)
		{
			return Refuse(VideoImportResult::BadCall, "Import video: the most seconds asked, " + std::to_string((long long)mostSeconds) + ", is not 1 to " + std::to_string((long long)kVideoImportLongestSeconds), why);
		}

		if (width == 0 || height == 0 || width > kVideoImportMostSide || height > kVideoImportMostSide)
		{
			return Refuse(VideoImportResult::TooLarge, "Import video: its picture is " + std::to_string(width) + " x " + std::to_string(height) + "; at most " + std::to_string(kVideoImportMostSide) +
				" a side is taken", why);
		}

		if (length > mostSeconds * kVideoImportTimeUnitsPerSecond)
		{
			return Refuse(VideoImportResult::TooLong, "Import video: it is " + std::to_string((long long)(length / kVideoImportTimeUnitsPerSecond)) + " s long; at most " +
				std::to_string((long long)mostSeconds) + " s is taken" + (mostSeconds < kVideoImportLongestSeconds ? " (mp4-to-smk: --max-seconds raises it, up to " + std::to_string((long long)kVideoImportLongestSeconds) + ")" : std::string()), why);
		}

		return VideoImportResult::Done;
	}

	bool VideoImport_LetterboxArea(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, int32_t area[4])
	{
		if (!area || width == 0 || height == 0 || width > kVideoImportMostSide || height > kVideoImportMostSide)
		{
			return false;
		}

		if (parWidth == 0 || parHeight == 0)
		{
			parWidth = parHeight = 1;
		}

		if (parWidth > 16 * (uint64_t)parHeight || parHeight > 16 * (uint64_t)parWidth)
		{
			return false;
		}

		// the picture's shape as it is seen (its pixels' shape counted), against 128 : 96
		double const shape = ((double)width * parWidth) / ((double)height * parHeight);
		int32_t fitWidth = kVideoImportWidth;
		int32_t fitHeight = kVideoImportHeight;

		if (shape * kVideoImportHeight >= (double)kVideoImportWidth)
		{
			fitHeight = (int32_t)floor((double)kVideoImportWidth / shape + 0.5);
		}
		else
		{
			fitWidth = (int32_t)floor((double)kVideoImportHeight * shape + 0.5);
		}

		fitWidth = fitWidth < 1 ? 1 : (fitWidth > kVideoImportWidth ? kVideoImportWidth : fitWidth);
		fitHeight = fitHeight < 1 ? 1 : (fitHeight > kVideoImportHeight ? kVideoImportHeight : fitHeight);
		area[0] = (kVideoImportWidth - fitWidth) / 2;
		area[1] = (kVideoImportHeight - fitHeight) / 2;
		area[2] = fitWidth;
		area[3] = fitHeight;
		return true;
	}

	namespace
	{
		// The source pixels one of `count` output pixels covers along a side of `size`, each with the share of the output pixel it covers (the shares add to 1)
		struct Share
		{
			uint32_t first;
			std::vector<double> weights;
		};

		// the part of the side used: from `start`, `span` long (the whole side, or crop's window)
		std::vector<Share> Shares(double start, double span, uint32_t size, int32_t count)
		{
			std::vector<Share> shares((size_t)count);
			double const step = span / (double)count;

			for (int32_t at = 0; at < count; at++)
			{
				double const from = start + at * step;
				double const to = start + (at + 1) * step;
				uint32_t first = (uint32_t)floor(from);
				uint32_t last = (uint32_t)ceil(to);

				first = first >= size ? size - 1 : first;
				last = last > size ? size : last;
				last = last <= first ? first + 1 : last;
				shares[(size_t)at].first = first;

				for (uint32_t pixel = first; pixel < last; pixel++)
				{
					double const lo = (double)pixel > from ? (double)pixel : from;
					double const hi = (double)(pixel + 1) < to ? (double)(pixel + 1) : to;

					shares[(size_t)at].weights.push_back(hi > lo ? (hi - lo) / step : 0.0);
				}
			}

			return shares;
		}

		uint8_t Byte(double value)
		{
			double const nearest = floor(value + 0.5);

			return (uint8_t)(nearest < 0.0 ? 0.0 : (nearest > 255.0 ? 255.0 : nearest));
		}
	}

	bool VideoImport_CropWindow(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, double window[4])
	{
		int32_t area[4];

		if (!window || !VideoImport_LetterboxArea(width, height, parWidth, parHeight, area))
		{
			return false;
		}

		if (parWidth == 0 || parHeight == 0)
		{
			parWidth = parHeight = 1;
		}

		// as seen, the picture is width x parWidth / parHeight wide; the part of 128 : 96's shape: the whole height of a wider picture, the whole width of a taller one
		double const seenWidth = (double)width * parWidth / parHeight;
		double const target = (double)kVideoImportWidth / (double)kVideoImportHeight;

		if (seenWidth >= (double)height * target)
		{
			double const used = (double)height * target * parHeight / parWidth;

			window[0] = ((double)width - used) / 2.0;
			window[1] = 0.0;
			window[2] = used;
			window[3] = (double)height;
		}
		else
		{
			double const used = seenWidth / target;

			window[0] = 0.0;
			window[1] = ((double)height - used) / 2.0;
			window[2] = (double)width;
			window[3] = used;
		}

		return true;
	}

	bool VideoImport_IsAvpShape(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight)
	{
		int32_t area[4];

		return VideoImport_LetterboxArea(width, height, parWidth, parHeight, area) && area[0] == 0 && area[1] == 0 && area[2] == kVideoImportWidth && area[3] == kVideoImportHeight;
	}

	bool VideoImport_Letterbox(uint8_t const *top, uint32_t width, uint32_t height, int64_t pitch, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit, uint8_t *out)
	{
		int32_t area[4];
		double window[4] = { 0.0, 0.0, (double)width, (double)height };

		if (!top || !out || !VideoImport_LetterboxArea(width, height, parWidth, parHeight, area))
		{
			return false;
		}

		if (fit == VideoImportFit::Crop)
		{
			if (!VideoImport_CropWindow(width, height, parWidth, parHeight, window))
			{
				return false;
			}

			area[0] = 0;
			area[1] = 0;
			area[2] = kVideoImportWidth;
			area[3] = kVideoImportHeight;
		}

		int64_t const rowBytes = (int64_t)width * 4;

		if ((pitch >= 0 ? pitch : -pitch) < rowBytes)
		{
			return false;
		}

		std::vector<Share> const across = Shares(window[0], window[2], width, area[2]);
		std::vector<Share> const down = Shares(window[1], window[3], height, area[3]);

		// across first: each source row the window covers to area[2] pixels (B, G, R as doubles), then down
		uint32_t const firstRow = down.front().first;
		uint32_t const endRow = down.back().first + (uint32_t)down.back().weights.size();
		std::vector<double> rows((size_t)(endRow - firstRow) * area[2] * 3);

		for (uint32_t y = firstRow; y < endRow; y++)
		{
			uint8_t const *const row = top + (int64_t)y * pitch;

			for (int32_t x = 0; x < area[2]; x++)
			{
				Share const &share = across[(size_t)x];
				double sums[3] = { 0.0, 0.0, 0.0 };

				for (size_t i = 0; i < share.weights.size(); i++)
				{
					uint8_t const *const pixel = row + (size_t)(share.first + i) * 4;

					sums[0] += share.weights[i] * pixel[0];
					sums[1] += share.weights[i] * pixel[1];
					sums[2] += share.weights[i] * pixel[2];
				}

				double *const into = &rows[((size_t)(y - firstRow) * area[2] + x) * 3];

				into[0] = sums[0];
				into[1] = sums[1];
				into[2] = sums[2];
			}
		}

		memset(out, 0, (size_t)kVideoImportFrameBytes);

		for (int32_t y = 0; y < area[3]; y++)
		{
			Share const &share = down[(size_t)y];

			for (int32_t x = 0; x < area[2]; x++)
			{
				double sums[3] = { 0.0, 0.0, 0.0 };

				for (size_t i = 0; i < share.weights.size(); i++)
				{
					double const *const from = &rows[((size_t)(share.first + i - firstRow) * area[2] + x) * 3];

					sums[0] += share.weights[i] * from[0];
					sums[1] += share.weights[i] * from[1];
					sums[2] += share.weights[i] * from[2];
				}

				uint8_t *const pixel = out + ((size_t)(area[1] + y) * kVideoImportWidth + (size_t)(area[0] + x)) * 3;

				pixel[0] = Byte(sums[2]);   // R
				pixel[1] = Byte(sums[1]);   // G
				pixel[2] = Byte(sums[0]);   // B
			}
		}

		return true;
	}

	int64_t VideoImport_TicksBefore(int64_t *next, int64_t time)
	{
		if (!next)
		{
			return 0;
		}

		int64_t const most = (kVideoImportLongestSeconds + 1) * kVideoImportTimeUnitsPerSecond;

		time = time < 0 ? 0 : (time > most ? most : time);

		// tick k (at k x 10^7 / 15, cut to a whole 100 ns) is before time when k x 10^7 < 15 x time: so the ticks before it are 0 .. ceil(15 time / 10^7) - 1
		int64_t const scaled = time * kVideoImportFramesPerSecond;
		int64_t const end = (scaled + kVideoImportTimeUnitsPerSecond - 1) / kVideoImportTimeUnitsPerSecond;

		if (end <= *next)
		{
			return 0;
		}

		int64_t const count = end - *next;

		*next = end;
		return count;
	}

	void VideoImport_SoundTo8(int16_t const *samples, size_t count, std::vector<uint8_t> *out)
	{
		if (!samples || !out)
		{
			return;
		}

		out->reserve(out->size() + count);

		for (size_t i = 0; i < count; i++)
		{
			int32_t const value = ((int32_t)samples[i] + 32768 + 128) >> 8;   // the nearest of 256 steps, a half going up

			out->push_back((uint8_t)(value > 255 ? 255 : value));
		}
	}

	namespace
	{
		// the largest 16-bit sample that still comes out below 255 + 1 in 8 bits ((s + 32896) >> 8 <= 255): the gain's ceiling, both ways
		double const kMostUnclipped = 32639.0;

		bool IsRealNumber(double value)
		{
			uint64_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return (bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;   // judged by its bits: /fp:fast may take NaN for a number
		}
	}

	double VideoImport_LevelGain(int16_t const *samples, size_t count, double targetRms)
	{
		if (!samples || count == 0 || !IsRealNumber(targetRms) || !(targetRms > 0.0))
		{
			return 1.0;
		}

		double power = 0.0;
		int32_t loudest = 0;

		for (size_t i = 0; i < count; i++)
		{
			int32_t const value = samples[i];
			int32_t const size = value < 0 ? -value : value;

			power += (double)value * value;
			loudest = size > loudest ? size : loudest;
		}

		double const rms = sqrt(power / (double)count);

		if (loudest == 0 || !(rms > 0.0))
		{
			return 1.0;
		}

		double const wanted = targetRms * 256.0 / rms;
		double const most = kMostUnclipped / (double)loudest;

		return wanted < most ? wanted : most;
	}

	void VideoImport_SoundTo8Gained(int16_t const *samples, size_t count, double gain, std::vector<uint8_t> *out)
	{
		if (!samples || !out)
		{
			return;
		}

		if (!IsRealNumber(gain) || !(gain > 0.0))
		{
			gain = 1.0;
		}

		out->reserve(out->size() + count);

		for (size_t i = 0; i < count; i++)
		{
			double const scaled = floor((double)samples[i] * gain + 0.5);
			int16_t const held = (int16_t)(scaled > 32767.0 ? 32767.0 : (scaled < -32768.0 ? -32768.0 : scaled));

			VideoImport_SoundTo8(&held, 1, out);
		}
	}

	double VideoImport_Loudness8(std::vector<uint8_t> const &sound)
	{
		if (sound.empty())
		{
			return 0.0;
		}

		double power = 0.0;

		for (uint8_t const value : sound)
		{
			power += ((double)value - 128.0) * ((double)value - 128.0);
		}

		return sqrt(power / (double)sound.size());
	}

	// ---- THE WHOLE READ, through Media Foundation

	namespace
	{
		using Microsoft::WRL::ComPtr;

		// COM on this thread for the read (a thread already in COM keeps its own kind), and Media Foundation started and stopped round it
		struct ComSession
		{
			bool uninitialise = false;
			ComSession() { uninitialise = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
			~ComSession() { if (uninitialise) CoUninitialize(); }
		};

		struct MediaFoundationSession
		{
			bool on = false;
			MediaFoundationSession() { on = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)); }
			~MediaFoundationSession() { if (on) MFShutdown(); }
		};

		std::string Code(HRESULT hr)
		{
			char words[16];

			snprintf(words, sizeof(words), "0x%08lx", (unsigned long)hr);
			return words;
		}

		// The most sound kept before the fit to the frames: the read's most seconds and two more (a sound a little longer than its picture is cut later)
		size_t MostSoundSamples(int64_t mostSeconds)
		{
			return (size_t)(mostSeconds + 2) * kVideoImportSoundRate;
		}

		struct PictureShape
		{
			uint32_t width = 0;          // the decoded frame (a decoder's frame can be larger than the picture: its rows and columns made up to whole blocks)
			uint32_t height = 0;
			uint32_t left = 0;           // the picture in it (MF_MT_MINIMUM_DISPLAY_APERTURE): only this is shown, the rest is the decoder's padding
			uint32_t top = 0;
			uint32_t shownWidth = 0;
			uint32_t shownHeight = 0;
			uint32_t parWidth = 1;
			uint32_t parHeight = 1;
			int64_t stride = 0;   // the type's own (negative: bottom row first), for a buffer that is not a 2D one
		};

		// The part of the frame that is the picture: the type's minimum display aperture, else its geometric aperture, else all of it; an aperture that doesn't lie inside the frame is
		// taken as none
		void ReadShownPart(IMFMediaType *type, PictureShape *shape)
		{
			GUID const kinds[2] = { MF_MT_MINIMUM_DISPLAY_APERTURE, MF_MT_GEOMETRIC_APERTURE };

			shape->left = 0;
			shape->top = 0;
			shape->shownWidth = shape->width;
			shape->shownHeight = shape->height;

			for (GUID const &kind : kinds)
			{
				MFVideoArea area = {};
				UINT32 size = 0;

				if (FAILED(type->GetBlob(kind, reinterpret_cast<UINT8 *>(&area), sizeof(area), &size)) || size != sizeof(area))
				{
					continue;
				}

				int64_t const left = area.OffsetX.value, top = area.OffsetY.value, width = area.Area.cx, height = area.Area.cy;

				if (left >= 0 && top >= 0 && width > 0 && height > 0 && left + width <= (int64_t)shape->width && top + height <= (int64_t)shape->height)
				{
					shape->left = (uint32_t)left;
					shape->top = (uint32_t)top;
					shape->shownWidth = (uint32_t)width;
					shape->shownHeight = (uint32_t)height;
					return;
				}
			}
		}

		VideoImportResult ReadPictureShape(IMFSourceReader *reader, DWORD stream, int64_t length, int64_t mostSeconds, PictureShape *shape, std::string *why)
		{
			ComPtr<IMFMediaType> type;
			UINT32 width = 0, height = 0;

			if (FAILED(reader->GetCurrentMediaType(stream, &type)) || FAILED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height)))
			{
				return Refuse(VideoImportResult::BadPicture, "Import video: Media Foundation gives no picture size", why);
			}

			VideoImportResult const bounds = VideoImport_CheckStream(width, height, length, why, mostSeconds);

			if (bounds != VideoImportResult::Done)
			{
				return bounds;
			}

			UINT32 parWidth = 1, parHeight = 1;

			if (FAILED(MFGetAttributeRatio(type.Get(), MF_MT_PIXEL_ASPECT_RATIO, &parWidth, &parHeight)) || parWidth == 0 || parHeight == 0 || parWidth > 16 * (uint64_t)parHeight ||
				parHeight > 16 * (uint64_t)parWidth)
			{
				parWidth = parHeight = 1;   // none, or past 1 : 16: taken as square (the picture is still shown, maybe the wrong shape)
			}

			UINT32 stride = 0;

			shape->width = width;
			shape->height = height;
			shape->parWidth = parWidth;
			shape->parHeight = parHeight;
			shape->stride = SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)) ? (int64_t)(INT32)stride : (int64_t)width * 4;
			ReadShownPart(type.Get(), shape);
			return VideoImportResult::Done;
		}

		// The shown part of a frame whose top row is at `top`, rows `pitch` bytes apart, fitted into out
		bool FitShown(uint8_t const *top, int64_t pitch, PictureShape const &shape, VideoImportFit fit, uint8_t *out)
		{
			uint8_t const *const shown = top + (int64_t)shape.top * pitch + (int64_t)shape.left * 4;

			return VideoImport_Letterbox(shown, shape.shownWidth, shape.shownHeight, pitch, shape.parWidth, shape.parHeight, fit, out);
		}

		// One decoded picture fitted into out; false for a buffer shorter than its picture
		bool FitPicture(IMFSample *sample, PictureShape const &shape, VideoImportFit fit, uint8_t *out)
		{
			ComPtr<IMFMediaBuffer> first;
			DWORD buffers = 0;

			// a 2D buffer knows its own rows (the first row and the step to the next): used when the picture is in one
			if (SUCCEEDED(sample->GetBufferCount(&buffers)) && buffers == 1 && SUCCEEDED(sample->GetBufferByIndex(0, &first)))
			{
				ComPtr<IMF2DBuffer> rows;

				if (SUCCEEDED(first.As(&rows)))
				{
					BYTE *top = nullptr;
					LONG pitch = 0;

					if (SUCCEEDED(rows->Lock2D(&top, &pitch)))
					{
						bool const fitted = (pitch >= 0 ? pitch : -(int64_t)pitch) >= (int64_t)shape.width * 4 && FitShown(top, pitch, shape, fit, out);

						rows->Unlock2D();
						return fitted;
					}
				}
			}

			// otherwise one run of bytes, rows of the type's stride (bottom row first when it is negative), and long enough for all of them
			ComPtr<IMFMediaBuffer> whole;
			BYTE *bytes = nullptr;
			DWORD length = 0;

			if (FAILED(sample->ConvertToContiguousBuffer(&whole)) || FAILED(whole->Lock(&bytes, nullptr, &length)))
			{
				return false;
			}

			uint64_t const step = (uint64_t)(shape.stride < 0 ? -shape.stride : shape.stride);
			bool fitted = false;

			if (step >= (uint64_t)shape.width * 4 && (uint64_t)length >= step * (shape.height - 1) + (uint64_t)shape.width * 4)
			{
				BYTE const *const top = shape.stride < 0 ? bytes + step * (shape.height - 1) : bytes;

				fitted = FitShown(top, shape.stride, shape, fit, out);
			}

			whole->Unlock();
			return fitted;
		}

		// Media Foundation's Audio Resampler (a DSP that is part of Windows): the sound as decoded in, 22050 Hz 16-bit mono out, then 8 bits
		struct Resampler
		{
			ComPtr<IMFTransform> transform;
			std::vector<int16_t> *sound = nullptr;   // 16-bit, 22050 Hz, mono
			size_t mostSamples = 0;                  // the read's bound (MostSoundSamples)

			bool Start(IMFMediaType *decoded, std::string *why)
			{
				ComPtr<IMFMediaType> wanted;

				if (FAILED(CoCreateInstance(CLSID_CResamplerMediaObject, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&transform))))
				{
					Refuse(VideoImportResult::BadSound, "Import video: Windows' Audio Resampler isn't there", why);
					return false;
				}

				ComPtr<IWMResamplerProps> quality;

				if (SUCCEEDED(transform.As(&quality)))
				{
					quality->SetHalfFilterLength(60);   // its best (1 .. 60)
				}

				HRESULT hr = transform->SetInputType(0, decoded, 0);

				if (SUCCEEDED(hr))
				{
					hr = MFCreateMediaType(&wanted);
				}

				if (SUCCEEDED(hr))
				{
					wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
					wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
					wanted->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
					wanted->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)kVideoImportSoundRate);
					wanted->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
					wanted->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2);
					wanted->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32)kVideoImportSoundRate * 2);
					wanted->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
					hr = transform->SetOutputType(0, wanted.Get(), 0);
				}

				if (SUCCEEDED(hr))
				{
					hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
				}

				if (SUCCEEDED(hr))
				{
					hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
				}

				if (FAILED(hr))
				{
					Refuse(VideoImportResult::BadSound, "Import video: Windows' Audio Resampler won't take this sound (" + Code(hr) + ")", why);
					return false;
				}

				return true;
			}

			// everything it has ready, to 8 bits; false on an error or past the most sound
			bool Pull()
			{
				MFT_OUTPUT_STREAM_INFO info = {};

				if (FAILED(transform->GetOutputStreamInfo(0, &info)))
				{
					return false;
				}

				bool const itsOwn = (info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;

				for (;;)
				{
					ComPtr<IMFSample> ours;
					ComPtr<IMFMediaBuffer> room;

					if (!itsOwn && (FAILED(MFCreateSample(&ours)) || FAILED(MFCreateMemoryBuffer(info.cbSize > 65536 ? info.cbSize : 65536, &room)) || FAILED(ours->AddBuffer(room.Get()))))
					{
						return false;
					}

					MFT_OUTPUT_DATA_BUFFER data = {};
					DWORD status = 0;

					data.dwStreamID = 0;
					data.pSample = ours.Get();

					HRESULT const hr = transform->ProcessOutput(0, 1, &data, &status);

					if (data.pEvents)
					{
						data.pEvents->Release();
					}

					ComPtr<IMFSample> given;

					if (itsOwn && data.pSample)
					{
						given.Attach(data.pSample);
					}

					if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						return true;
					}

					if (FAILED(hr))
					{
						return false;
					}

					IMFSample *const out = itsOwn ? given.Get() : ours.Get();
					ComPtr<IMFMediaBuffer> whole;
					BYTE *bytes = nullptr;
					DWORD length = 0;

					if (!out || FAILED(out->ConvertToContiguousBuffer(&whole)) || FAILED(whole->Lock(&bytes, nullptr, &length)))
					{
						return false;
					}

					sound->insert(sound->end(), reinterpret_cast<int16_t const *>(bytes), reinterpret_cast<int16_t const *>(bytes) + length / 2);
					whole->Unlock();

					if (sound->size() > mostSamples)
					{
						return false;
					}
				}
			}

			bool Push(IMFSample *decoded)
			{
				HRESULT hr = transform->ProcessInput(0, decoded, 0);

				if (hr == MF_E_NOTACCEPTING)
				{
					if (!Pull())
					{
						return false;
					}

					hr = transform->ProcessInput(0, decoded, 0);
				}

				return SUCCEEDED(hr) && Pull();
			}

			bool Finish()
			{
				return SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0)) && SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0)) && Pull();
			}
		};

		// The file's own numbers of its first picture stream and its first sound stream (-1 for none)
		void FindStreams(IMFSourceReader *reader, DWORD *picture, DWORD *sound, bool *havePicture, bool *haveSound)
		{
			*havePicture = *haveSound = false;

			for (DWORD stream = 0; stream < 64 && !(*havePicture && *haveSound); stream++)
			{
				ComPtr<IMFMediaType> type;
				GUID major = GUID_NULL;

				if (reader->GetNativeMediaType(stream, 0, &type) == MF_E_INVALIDSTREAMNUMBER)
				{
					break;
				}

				if (!type || FAILED(type->GetMajorType(&major)))
				{
					continue;
				}

				if (major == MFMediaType_Video && !*havePicture)
				{
					*picture = stream;
					*havePicture = true;
				}
				else if (major == MFMediaType_Audio && !*haveSound)
				{
					*sound = stream;
					*haveSound = true;
				}
			}
		}
	}

	namespace
	{
		// The file opened for reading: its reader, its length, its first picture stream set to RGB32 and that picture's shape checked, its first sound stream found (not yet chosen)
		struct Opened
		{
			ComPtr<IMFSourceReader> reader;
			int64_t length = -1;
			DWORD pictureStream = 0;
			DWORD soundStream = 0;
			bool haveSound = false;
			PictureShape shape;
		};

		// The call's checks and the sessions both reads need (COM and Media Foundation stay on while the caller holds them)
		VideoImportResult CheckBytes(std::vector<uint8_t> const &bytes, wchar_t const *contentType, void const *place, MediaFoundationSession const &mediaFoundation, std::string *why)
		{
			if (!place || !contentType || bytes.empty())
			{
				return Refuse(VideoImportResult::BadCall, "Import video: nothing to read", why);
			}

			if (bytes.size() > kVideoImportMostBytes)
			{
				return Refuse(VideoImportResult::TooBig, "Import video: the file is too big", why);
			}

			if (!mediaFoundation.on)
			{
				return Refuse(VideoImportResult::NoMediaFoundation, "Import video: Windows' Media Foundation won't start (an N edition of Windows needs its Media Feature Pack)", why);
			}

			return VideoImportResult::Done;
		}
	}

	static VideoImportResult OpenForRead(std::vector<uint8_t> const &bytes, wchar_t const *contentType, int64_t mostSeconds, Opened *opened, std::string *why)
	{
		// the bytes as a stream in memory: Media Foundation never opens the path
		HGLOBAL const global = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
		void *const into = global ? GlobalLock(global) : nullptr;

		if (!into)
		{
			if (global)
			{
				GlobalFree(global);
			}

			return Refuse(VideoImportResult::CantRead, "Import video: no memory for the file", why);
		}

		memcpy(into, bytes.data(), bytes.size());
		GlobalUnlock(global);

		ComPtr<IStream> stream;

		if (FAILED(CreateStreamOnHGlobal(global, TRUE, &stream)))
		{
			GlobalFree(global);
			return Refuse(VideoImportResult::CantRead, "Import video: no memory for the file", why);
		}

		ComPtr<IMFByteStream> byteStream;
		ComPtr<IMFAttributes> byteStreamAttributes;
		ComPtr<IMFAttributes> readerAttributes;
		ComPtr<IMFSourceReader> reader;
		HRESULT hr = MFCreateMFByteStreamOnStream(stream.Get(), &byteStream);

		if (SUCCEEDED(hr) && SUCCEEDED(byteStream.As(&byteStreamAttributes)))
		{
			byteStreamAttributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, contentType);
		}

		if (SUCCEEDED(hr))
		{
			hr = MFCreateAttributes(&readerAttributes, 1);
		}

		if (SUCCEEDED(hr))
		{
			readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);   // its own converter to RGB32
			hr = MFCreateSourceReaderFromByteStream(byteStream.Get(), readerAttributes.Get(), &reader);
		}

		if (FAILED(hr))
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: Windows can't read it as a video (" + Code(hr) + ")", why);
		}

		PROPVARIANT duration;

		PropVariantInit(&duration);

		if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration)) && duration.vt == VT_UI8)
		{
			opened->length = duration.uhVal.QuadPart > (ULONGLONG)INT64_MAX ? INT64_MAX : (int64_t)duration.uhVal.QuadPart;
		}

		PropVariantClear(&duration);

		bool havePicture = false;

		FindStreams(reader.Get(), &opened->pictureStream, &opened->soundStream, &havePicture, &opened->haveSound);

		if (!havePicture)
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: there's no picture in it", why);
		}

		reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
		reader->SetStreamSelection(opened->pictureStream, TRUE);

		// the picture as RGB32 (Media Foundation's own converter)
		ComPtr<IMFMediaType> rgb;

		hr = MFCreateMediaType(&rgb);

		if (SUCCEEDED(hr))
		{
			rgb->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			rgb->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			hr = reader->SetCurrentMediaType(opened->pictureStream, nullptr, rgb.Get());
		}

		if (FAILED(hr))
		{
			return Refuse(VideoImportResult::BadPicture, "Import video: Windows can't decode its picture (" + Code(hr) + ")", why);
		}

		VideoImportResult const shaped = ReadPictureShape(reader.Get(), opened->pictureStream, opened->length, mostSeconds, &opened->shape, why);

		if (shaped == VideoImportResult::Done)
		{
			opened->reader = reader;
		}

		return shaped;
	}

	VideoImportResult VideoImport_Probe(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportInfo *info, std::string *why, int64_t mostSeconds)
	{
		ComSession const com;
		MediaFoundationSession const mediaFoundation;
		VideoImportResult result = CheckBytes(bytes, contentType, info, mediaFoundation, why);
		Opened opened;

		if (result == VideoImportResult::Done)
		{
			result = OpenForRead(bytes, contentType, mostSeconds, &opened, why);
		}

		if (result != VideoImportResult::Done)
		{
			return result;
		}

		info->width = opened.shape.shownWidth;
		info->height = opened.shape.shownHeight;
		info->parWidth = opened.shape.parWidth;
		info->parHeight = opened.shape.parHeight;
		info->length = opened.length;
		info->hasSound = opened.haveSound;
		return VideoImportResult::Done;
	}

	VideoImportResult VideoImport_ReadBytes(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportFit fit, VideoImportClip *clip, std::string *why, int64_t mostSeconds)
	{
		ComSession const com;
		MediaFoundationSession const mediaFoundation;
		VideoImportResult result = CheckBytes(bytes, contentType, clip, mediaFoundation, why);
		Opened opened;

		if (result == VideoImportResult::Done)
		{
			result = OpenForRead(bytes, contentType, mostSeconds, &opened, why);
		}

		if (result != VideoImportResult::Done)
		{
			return result;
		}

		ComPtr<IMFSourceReader> const reader = opened.reader;
		DWORD const pictureStream = opened.pictureStream;
		DWORD const soundStream = opened.soundStream;
		bool const haveSound = opened.haveSound;
		PictureShape shape = opened.shape;
		VideoImportClip read;
		HRESULT hr = S_OK;

		read.length = opened.length;
		read.sourceWidth = shape.shownWidth;
		read.sourceHeight = shape.shownHeight;

		// the sound as 16-bit PCM, then the resampler
		Resampler resampler;

		resampler.sound = &read.sound16;
		resampler.mostSamples = MostSoundSamples(mostSeconds);

		// the frames' room taken at once when the file gives its length (a 32-bit program: a block that grows by doubling may find no room)
		if (read.length > 0)
		{
			read.frames.reserve((size_t)((read.length * kVideoImportFramesPerSecond) / kVideoImportTimeUnitsPerSecond + 2) * kVideoImportFrameBytes);
		}

		if (haveSound)
		{
			ComPtr<IMFMediaType> pcm;
			ComPtr<IMFMediaType> decoded;

			reader->SetStreamSelection(soundStream, TRUE);
			hr = MFCreateMediaType(&pcm);

			if (SUCCEEDED(hr))
			{
				pcm->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
				pcm->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
				pcm->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
				hr = reader->SetCurrentMediaType(soundStream, nullptr, pcm.Get());
			}

			if (SUCCEEDED(hr))
			{
				hr = reader->GetCurrentMediaType(soundStream, &decoded);
			}

			UINT32 channels = 0, rate = 0, bits = 0;

			if (FAILED(hr) || FAILED(decoded->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels)) || FAILED(decoded->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate)) ||
				FAILED(decoded->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits)) || channels < 1 || channels > 8 || rate < 1000 || rate > 384000 || bits != 16)
			{
				return Refuse(VideoImportResult::BadSound, "Import video: Windows can't decode its sound (" + Code(hr) + ")", why);
			}

			if (!resampler.Start(decoded.Get(), why))
			{
				return VideoImportResult::BadSound;
			}
		}

		std::vector<uint8_t> pending((size_t)kVideoImportFrameBytes);
		std::vector<uint8_t> fitted((size_t)kVideoImportFrameBytes);
		bool havePending = false;
		int64_t nextTick = 0;
		int64_t lastStart = 0;
		int64_t lastLength = 0;
		bool pictureDone = false;
		bool soundDone = !haveSound;
		bool soundStarted = false;
		int64_t const mostTime = mostSeconds * kVideoImportTimeUnitsPerSecond;
		size_t const mostSamples = MostSoundSamples(mostSeconds);

		auto addFrames = [&read](std::vector<uint8_t> const &frame, int64_t count) {
			for (int64_t i = 0; i < count; i++)
			{
				read.frames.insert(read.frames.end(), frame.begin(), frame.end());
				read.frameCount++;
			}
		};

		while (!pictureDone || !soundDone)
		{
			DWORD streamIndex = 0, flags = 0;
			LONGLONG start = 0;
			ComPtr<IMFSample> sample;

			hr = reader->ReadSample((DWORD)MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &start, &sample);

			bool const isPicture = streamIndex == pictureStream;

			if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR))
			{
				return Refuse(isPicture ? VideoImportResult::BadPicture : VideoImportResult::BadSound, std::string("Import video: Windows stopped decoding its ") + (isPicture ? "picture" : "sound") + " (" + Code(hr) + ")", why);
			}

			if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) && isPicture)
			{
				result = ReadPictureShape(reader.Get(), pictureStream, read.length, mostSeconds, &shape, why);

				if (result != VideoImportResult::Done)
				{
					return result;
				}

				read.sourceWidth = shape.shownWidth;
				read.sourceHeight = shape.shownHeight;
			}

			if (sample && start > mostTime)
			{
				return Refuse(VideoImportResult::TooLong, "Import video: it runs past " + std::to_string((long long)mostSeconds) + " s; at most that is taken", why);
			}

			if (sample && isPicture)
			{
				if (!FitPicture(sample.Get(), shape, fit, fitted.data()))
				{
					return Refuse(VideoImportResult::BadPicture, "Import video: a decoded picture is shorter than its size says", why);
				}

				// the ticks before this picture: the one before it fills them (the first picture fills those before itself)
				int64_t const count = VideoImport_TicksBefore(&nextTick, start);

				addFrames(havePending ? pending : fitted, count);
				pending.swap(fitted);
				havePending = true;

				LONGLONG length = 0;

				lastStart = start;
				lastLength = SUCCEEDED(sample->GetSampleDuration(&length)) && length > 0 ? length : 0;
			}
			else if (sample && streamIndex == soundStream && haveSound)
			{
				// a sound that starts late: silence before it, as long as the gap
				if (!soundStarted && start > 0)
				{
					size_t const gap = (size_t)((double)start * kVideoImportSoundRate / (double)kVideoImportTimeUnitsPerSecond);

					read.sound16.assign(gap < mostSamples ? gap : mostSamples, 0);
				}

				soundStarted = true;

				if (!resampler.Push(sample.Get()))
				{
					return Refuse(read.sound16.size() > mostSamples ? VideoImportResult::TooLong : VideoImportResult::BadSound, "Import video: its sound couldn't be resampled, or runs too long", why);
				}
			}

			if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
			{
				if (isPicture)
				{
					pictureDone = true;
				}
				else
				{
					soundDone = true;
				}
			}
		}

		if (!havePending)
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: no picture could be decoded", why);
		}

		if (haveSound && !resampler.Finish())
		{
			return Refuse(read.sound16.size() > mostSamples ? VideoImportResult::TooLong : VideoImportResult::BadSound, "Import video: its sound couldn't be resampled to the end", why);
		}

		// the last picture fills the ticks to its own end; when it gives no length, to the file's length or a tick on (the file's length alone would count a sound's padding: an
		// AAC track runs a little past its pictures)
		int64_t const tickLater = lastStart + kVideoImportTimeUnitsPerSecond / kVideoImportFramesPerSecond;
		int64_t const end = lastLength > 0 ? lastStart + lastLength : (read.length > tickLater ? read.length : tickLater);

		addFrames(pending, VideoImport_TicksBefore(&nextTick, end));

		// the sound fitted to the frames: 1470 samples each, silence put in at the end or the rest cut; then its 8 bits
		if (haveSound)
		{
			read.sound16.resize((size_t)read.frameCount * kVideoImportSamplesPerFrame, 0);
			VideoImport_SoundTo8(read.sound16.data(), read.sound16.size(), &read.sound);
		}

		*clip = std::move(read);

		if (why)
		{
			why->clear();
		}

		return VideoImportResult::Done;
	}

	VideoImportResult VideoImport_Read(std::wstring const &path, VideoImportFit fit, VideoImportClip *clip, std::string *why, int64_t mostSeconds)
	{
		if (!clip)
		{
			return Refuse(VideoImportResult::BadCall, "Import video: no place for the clip", why);
		}

		std::vector<uint8_t> bytes;
		VideoImportResult const read = VideoImport_ReadFile(path, &bytes, why);

		if (read != VideoImportResult::Done)
		{
			return read;
		}

		return VideoImport_ReadBytes(bytes, VideoImport_ContentType(path), fit, clip, why, mostSeconds);
	}
}
