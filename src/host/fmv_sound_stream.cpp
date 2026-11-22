// See fmv_sound_stream.h. Written 2026-11-21 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "fmv_sound_stream.h"

namespace host
{
	bool FmvSoundStream::Open(SmackerDllCalls const *dll, std::string const &path, model::SmkHeader const &header, std::string *words)
	{
		Close();

		model::SmkTrack const &track = header.tracks[0];

		if (!track.present)
		{
			*words = "it has no sound";
			return false;
		}

		if (!dll || !dll->Open || !dll->Close || !dll->GetNumAudioTracks || !dll->GetAudioTrackDetails || !dll->GetNextFrame || !dll->GetAudioData)
		{
			*words = "picture only: the decoder would not open its sound";
			return false;
		}

		SmackerHandle opened = dll->Open(path.c_str());

		if (!opened.isValid)
		{
			*words = "picture only: the decoder would not open its sound";
			return false;
		}

		// the decoder has to agree with the header we checked, and give a bounded buffer for a frame's sound
		SmackerAudioInfo const info = dll->GetNumAudioTracks(opened) > 0 ? dll->GetAudioTrackDetails(opened, 0) : SmackerAudioInfo{ 0, 0, 0, 0 };
		bool const agrees = info.sampleRate == track.rate && info.nChannels == (uint32_t)track.channels && info.bitsPerSample == (uint32_t)track.bits &&
			(info.bitsPerSample == 8 || info.bitsPerSample == 16) && (info.nChannels == 1 || info.nChannels == 2) && info.sampleRate > 0 && info.idealBufferSize > 0 &&
			info.idealBufferSize <= kFmvSoundMostFrameBuffer && header.frames > 0;

		if (!agrees)
		{
			dll->Close(opened);
			*words = "picture only: the decoder reads its sound differently from its header";
			return false;
		}

		calls = dll;
		handle = opened;
		open = true;
		frames = header.frames;
		framesRead = 0;
		rate = info.sampleRate;
		channels = info.nChannels;
		bits = info.bitsPerSample;
		frameBuffer = info.idealBufferSize;
		frameSound.assign(frameBuffer, 0);
		carry.clear();
		*words = "with its sound";
		return true;
	}

	bool FmvSoundStream::NextChunk(std::vector<uint8_t> *bytes)
	{
		bytes->clear();

		if (!open || framesRead >= frames)
		{
			return false;
		}

		size_t const sampleFrame = (size_t)channels * (bits / 8);
		size_t const wanted = (size_t)((uint64_t)rate * kFmvSoundChunkMs / 1000) * sampleFrame;   // a quarter of a second, in bytes

		bytes->swap(carry);   // what a sample frame left over at the last chunk's end comes first
		carry.clear();

		for (uint32_t decodedHere = 0; framesRead < frames && decodedHere < kFmvSoundMostFramesAChunk && bytes->size() < wanted; decodedHere++)
		{
			calls->GetNextFrame(handle);
			framesRead++;

			uint32_t got = calls->GetAudioData(handle, 0, (int16_t *)frameSound.data());

			got = got > frameBuffer ? frameBuffer : got;   // never more than the decoder's own buffer for a frame
			bytes->insert(bytes->end(), frameSound.begin(), frameSound.begin() + got);
		}

		// whole sample frames only: a cut one waits for the next chunk (the sound device takes whole frames)
		size_t const cut = bytes->size() % sampleFrame;

		if (cut)
		{
			carry.assign(bytes->end() - (ptrdiff_t)cut, bytes->end());
			bytes->resize(bytes->size() - cut);
		}

		return !bytes->empty() || framesRead < frames;
	}

	void FmvSoundStream::Close()
	{
		if (open && calls)
		{
			calls->Close(handle);
		}

		calls = nullptr;
		handle = { false, -1 };
		open = false;
		frames = framesRead = 0;
		rate = channels = bits = frameBuffer = 0;
		frameSound.clear();
		carry.clear();
	}
}
