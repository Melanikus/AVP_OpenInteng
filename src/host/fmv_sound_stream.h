// fmv_sound_stream.h: an FMV preview's SOUND, STREAMED (the owner 11-21, through Bishop: "her video is long so it got stuck for a few seconds, we need a loading video %").
// Written 2026-11-21 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the decoder gives a frame's sound only as it decodes that frame (smacker_dll.h). The preview first read the whole first track into one sound buffer before its picture
// started, which meant decoding EVERY frame of the film first: 2,471 frames for his 164.7 s film, seconds of the editor's own thread with its window not drawn.
// HOW: the sound is read here a few frames at a time, through a decoder handle of its own, in CHUNKS of about a quarter of a second. The preview queues the chunks on the sound
// device one after another and refills each one as it has been played (fmv_preview.cpp: OpenAL buffers queued on one source). So Play starts at once whatever the film's length,
// and nothing is left to load: no "Loading %" is needed.
// ONE THREAD: the decoder DLL is called only from the editor's own thread, as the picture's decoding is (its thread safety is not reviewed: Tao 1).
// HOSTILE INPUT: the decoder must agree with the header we checked (fmv_files.h) on the track's rate, channels and bits; its buffer for a frame is bounded; a frame never gives
// more than that buffer; a chunk holds whole sample frames only.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "smacker_dll.h"
#include "smk_header.h"

namespace host
{
	uint32_t const kFmvSoundChunkMs = 250;           // a chunk: about a quarter of a second of sound
	uint32_t const kFmvSoundChunks = 8;              // chunks on the sound device at once: up to 2 seconds ahead of what is heard
	uint32_t const kFmvSoundStartChunks = 4;         // queued before the picture starts: a second of sound, so the start never runs dry
	uint32_t const kFmvSoundMostFramesAChunk = 16;   // a chunk decodes at most this many frames, so one editor frame's work stays small whatever the film
	uint32_t const kFmvSoundMostRefillsAFrame = 2;   // chunks refilled in one editor frame, at most (half a second of sound: an editor frame is about 40 ms)
	uint32_t const kFmvSoundMostFrameBuffer = 16u * 1024 * 1024;   // the decoder's buffer for a frame's sound, at most

	class FmvSoundStream
	{
	public:
		// Opens path's first track through a handle of its own and checks it against the header. False with *words (for the status line) when there is no sound to play:
		// "it has no sound", "picture only: the decoder would not open its sound", "picture only: the decoder reads its sound differently from its header". Nothing is decoded yet.
		bool Open(SmackerDllCalls const *dll, std::string const &path, model::SmkHeader const &header, std::string *words);

		// The next chunk into *bytes (replaced): frames decoded until it holds about kFmvSoundChunkMs of sound, or kFmvSoundMostFramesAChunk frames, or the film ends; whole sample
		// frames only. False, with *bytes empty, once every frame has been read.
		bool NextChunk(std::vector<uint8_t> *bytes);

		void Close();

		bool IsOpen() const { return open; }
		bool Done() const { return !open || framesRead >= frames; }
		uint32_t FramesRead() const { return framesRead; }
		uint32_t Rate() const { return rate; }
		uint32_t Channels() const { return channels; }
		uint32_t Bits() const { return bits; }

	private:
		SmackerDllCalls const *calls = nullptr;
		SmackerHandle handle = { false, -1 };
		bool open = false;
		uint32_t frames = 0, framesRead = 0;
		uint32_t rate = 0, channels = 0, bits = 0, frameBuffer = 0;
		std::vector<uint8_t> frameSound;   // one frame's sound, as the decoder gives it
		std::vector<uint8_t> carry;        // the bytes of a sample frame cut by a chunk's end, put at the next chunk's start
	};
}
