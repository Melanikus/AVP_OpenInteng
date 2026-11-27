// video_make.cpp: what is done to a read video before it is encoded (video_make.h says what and why). Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng, the level
// code Vasquez's (11-18). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "video_make.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#include <windows.h>

#include "smk_encoder.h"

namespace
{
	double HeldVolume(double volumeDb)
	{
		if (_isnan(volumeDb))
		{
			return 0.0;   // not a number (asked by its bits: /fp:fast may take x == x as always true)
		}

		return volumeDb > host::kVideoMakeMostVolumeDb ? host::kVideoMakeMostVolumeDb : (volumeDb < -host::kVideoMakeMostVolumeDb ? -host::kVideoMakeMostVolumeDb : volumeDb);
	}

	std::string Narrow(std::wstring const &wide)
	{
		std::string narrow;

		for (wchar_t const c : wide)
		{
			narrow.push_back(c >= 0x20 && c < 0x7f ? (char)c : '?');
		}

		return narrow;
	}
}

int32_t host::VideoMake_FrameAt(double seconds, int32_t frameCount)
{
	if (_isnan(seconds) || seconds <= 0.0 || frameCount <= 0)
	{
		return 0;
	}

	double const frame = floor(seconds * kVideoImportFramesPerSecond + 0.5);

	return frame >= (double)frameCount ? frameCount : (int32_t)frame;
}

bool host::VideoMake_Cut(VideoImportClip *clip, int32_t fromFrame, int32_t toFrame)
{
	if (!clip || fromFrame < 0 || toFrame <= fromFrame || toFrame > clip->frameCount ||
		clip->frames.size() < (size_t)clip->frameCount * kVideoImportFrameBytes)
	{
		return false;
	}

	size_t const frameBytes = (size_t)kVideoImportFrameBytes;
	size_t const samples = (size_t)kVideoImportSamplesPerFrame;

	clip->frames.erase(clip->frames.begin() + (ptrdiff_t)((size_t)toFrame * frameBytes), clip->frames.end());
	clip->frames.erase(clip->frames.begin(), clip->frames.begin() + (ptrdiff_t)((size_t)fromFrame * frameBytes));

	// the sound: exactly the kept frames' samples (a sound shorter than its frames, which the read never gives, is cut to what is there)
	for (std::vector<uint8_t> *sound8 : { &clip->sound })
	{
		size_t const end = (size_t)toFrame * samples < sound8->size() ? (size_t)toFrame * samples : sound8->size();
		size_t const start = (size_t)fromFrame * samples < end ? (size_t)fromFrame * samples : end;

		sound8->erase(sound8->begin() + (ptrdiff_t)end, sound8->end());
		sound8->erase(sound8->begin(), sound8->begin() + (ptrdiff_t)start);
	}

	{
		std::vector<int16_t> &sound16 = clip->sound16;
		size_t const end = (size_t)toFrame * samples < sound16.size() ? (size_t)toFrame * samples : sound16.size();
		size_t const start = (size_t)fromFrame * samples < end ? (size_t)fromFrame * samples : end;

		sound16.erase(sound16.begin() + (ptrdiff_t)end, sound16.end());
		sound16.erase(sound16.begin(), sound16.begin() + (ptrdiff_t)start);
	}

	clip->frameCount = toFrame - fromFrame;
	return true;
}

void host::VideoMake_Level(VideoImportClip *clip, double level, double volumeDb, std::string *words)
{
	if (!clip || clip->sound16.empty())
	{
		if (words)
		{
			*words = "no sound";
		}

		return;
	}

	double const volume = HeldVolume(volumeDb);
	double const before = VideoImport_Loudness8(clip->sound);
	double const base = level > 0.0 ? level : before;   // the source's own level: what it has
	double const target = base * pow(10.0, volume / 20.0);

	// the source's own level and no volume: the sound as it came, its 8 bits untouched
	if (level <= 0.0 && volume == 0.0)
	{
		if (words)
		{
			char text[128];

			snprintf(text, sizeof(text), "sound level %.1f of 128 (RMS in 8-bit steps), the source's own", before);
			*words = text;
		}

		return;
	}

	double const gain = VideoImport_LevelGain(clip->sound16.data(), clip->sound16.size(), target);

	clip->sound.clear();
	VideoImport_SoundTo8Gained(clip->sound16.data(), clip->sound16.size(), gain, &clip->sound);

	if (words)
	{
		char text[192];

		snprintf(text, sizeof(text), "sound level %.1f -> %.1f of 128 (RMS in 8-bit steps; target %.1f, gain x%.2f, held so nothing clips)", before, VideoImport_Loudness8(clip->sound),
			target, gain);
		*words = text;
	}
}

std::string host::VideoMake_VolumeWords(bool sourceLevel, double volumeDb)
{
	double const volume = HeldVolume(volumeDb);
	std::string const base = sourceLevel ? "the video's own level" : "AvP's level";

	if (volume == 0.0)
	{
		return base;
	}

	char text[32];

	snprintf(text, sizeof(text), " %+.0f dB", volume);
	return base + text;
}

bool host::VideoMake_Save(std::wstring const &path, std::vector<uint8_t> const &smk, std::wstring const &backupFolder, std::wstring const &stamp, std::wstring *keptAs,
	std::string *why)
{
	if (keptAs)
	{
		keptAs->clear();
	}

	if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		return Smk_WriteNew(path, smk, why);
	}

	// a film there: kept first, under a name never used before (CopyFile fails rather than write over a backup)
	size_t const slash = path.find_last_of(L"\\/");
	std::wstring const name = slash == std::wstring::npos ? path : path.substr(slash + 1);
	std::wstring const backup = backupFolder + L"\\" + name + L"." + stamp;

	if (!CreateDirectoryW(backupFolder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		*why = "the backup folder " + Narrow(backupFolder) + " can't be made, so the film there is left as it was";
		return false;
	}

	if (!CopyFileW(path.c_str(), backup.c_str(), TRUE))
	{
		*why = GetLastError() == ERROR_FILE_EXISTS ? "a backup named " + Narrow(backup) + " is there already: it is never written over, so the film there is left as it was" :
			"the film there can't be kept as " + Narrow(backup) + ", so it is left as it was";
		return false;
	}

	if (!Smk_Replace(path, smk, why))
	{
		return false;
	}

	if (keptAs)
	{
		*keptAs = backup;
	}

	return true;
}
