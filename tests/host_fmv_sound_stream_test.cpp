// host_fmv_sound_stream_test: an FMV preview's sound, streamed (fmv_sound_stream.h), through a stand-in decoder with the real one's calls. Written 2026-11-21 by Melanikus (Vasquez) for
// AVP_OpenInteng (the owner's "her video is long so it got stuck for a few seconds, we need a loading video %"). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The stand-in plays his film's numbers: 2,471 frames at 15 a second, 22050 Hz 8-bit mono, so 1,470 bytes of sound a frame. Opening decodes no frame, a chunk only the few frames
// of a quarter of a second, and the whole track comes out in order, every byte once; a decoder that disagrees with the header, or gives too much, is refused or held to its buffer.

#include <stdio.h>

#include <string>
#include <vector>

#include "fmv_sound_stream.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checksFailed += good ? 0 : 1;
}

// THE STAND-IN DECODER: its film, and what was asked of it
namespace
{
	bool openWorks = true;
	uint32_t filmFrames = 2471, tracks = 1, bytesAFrame = 1470;
	SmackerAudioInfo details = { 22050, 1, 8, 2048 };
	uint32_t framesDecoded = 0, opened = 0, closed = 0;

	SmackerHandle FakeOpen(const char *)
	{
		opened++;
		framesDecoded = 0;
		return SmackerHandle{ openWorks, openWorks ? 7 : -1 };
	}

	void FakeClose(SmackerHandle &)
	{
		closed++;
	}

	uint32_t FakeTracks(SmackerHandle &)
	{
		return tracks;
	}

	SmackerAudioInfo FakeDetails(SmackerHandle &, uint32_t)
	{
		return details;
	}

	uint32_t FakeNextFrame(SmackerHandle &)
	{
		return framesDecoded < filmFrames ? ++framesDecoded : framesDecoded;
	}

	// a frame's sound: every byte the frame's number (its low byte), so the order can be read back; as many bytes as the film gives, written only up to the buffer the decoder said
	uint32_t FakeAudio(SmackerHandle &, uint32_t, int16_t *data)
	{
		uint8_t *const out = (uint8_t *)data;
		uint32_t const writable = bytesAFrame < details.idealBufferSize ? bytesAFrame : details.idealBufferSize;

		for (uint32_t i = 0; i < writable; i++)
		{
			out[i] = (uint8_t)(framesDecoded & 0xff);
		}

		return bytesAFrame;   // it may SAY more than its buffer: the stream must not read past it
	}

	host::SmackerDllCalls Calls()
	{
		host::SmackerDllCalls calls;

		calls.Open = &FakeOpen;
		calls.Close = &FakeClose;
		calls.GetNumAudioTracks = &FakeTracks;
		calls.GetAudioTrackDetails = &FakeDetails;
		calls.GetNextFrame = &FakeNextFrame;
		calls.GetAudioData = &FakeAudio;
		return calls;
	}

	model::SmkHeader HisFilm()
	{
		model::SmkHeader header;

		header.width = 128;
		header.height = 96;
		header.frames = 2471;
		header.msPerFrame = 66.66;
		header.tracks[0].present = true;
		header.tracks[0].bits = 8;
		header.tracks[0].channels = 1;
		header.tracks[0].rate = 22050;
		return header;
	}

	void Reset()
	{
		openWorks = true;
		filmFrames = 2471;
		tracks = 1;
		bytesAFrame = 1470;
		details = { 22050, 1, 8, 2048 };
		framesDecoded = opened = closed = 0;
	}
}

int main()
{
	bool did = false;   // a call made before the check that reads it: the check's words are built before its condition is (MSVC), so they show what the call left

	host::SmackerDllCalls const calls = Calls();

	// HIS FILM: opening decodes nothing; the first second, the few frames it takes; the whole track in order, every byte once
	{
		Reset();

		host::FmvSoundStream stream;
		std::string words;

		did = stream.Open(&calls, "message1.smk", HisFilm(), &words);
		Check(did && words == "with its sound" && stream.Rate() == 22050 && stream.Channels() == 1 && stream.Bits() == 8,
			"his film opens with its sound, 22050 Hz mono 8-bit (\"" + words + "\")");
		Check(framesDecoded == 0, "OPENING DECODES NO FRAME (the preview's first way decoded all 2,471 before it played): " + std::to_string(framesDecoded) + " decoded");

		std::vector<uint8_t> chunk;

		did = stream.NextChunk(&chunk);
		Check(did && framesDecoded == 4 && chunk.size() == 4 * 1470, "a chunk is a quarter of a second: 4 frames, " + std::to_string(chunk.size()) +
			" bytes (5,512 wanted, whole frames read), " + std::to_string(framesDecoded) + " frames decoded");

		for (uint32_t c = 1; c < host::kFmvSoundStartChunks; c++)
		{
			stream.NextChunk(&chunk);
		}

		Check(framesDecoded == 16, "the first second the preview queues before it plays (" + std::to_string(host::kFmvSoundStartChunks) + " chunks) decodes " +
			std::to_string(framesDecoded) + " frames of 2,471");

		stream.Close();
		Check(closed == 1, "closed, its handle let go");
		Reset();
		stream.Open(&calls, "message1.smk", HisFilm(), &words);

		std::vector<uint8_t> all;
		size_t chunks = 0, mostFrames = 0;
		bool inOrder = true;

		while (true)
		{
			uint32_t const before = framesDecoded;

			if (!stream.NextChunk(&chunk))
			{
				break;
			}

			chunks++;
			mostFrames = framesDecoded - before > mostFrames ? framesDecoded - before : mostFrames;
			all.insert(all.end(), chunk.begin(), chunk.end());

			if (chunks > 10000)
			{
				break;
			}
		}

		for (size_t i = 0; i < all.size() && inOrder; i++)
		{
			inOrder = all[i] == (uint8_t)((i / 1470 + 1) & 0xff);
		}

		Check(all.size() == (size_t)2471 * 1470 && inOrder, "the whole track comes out, in order, every byte once: " + std::to_string(all.size()) + " bytes of " +
			std::to_string(2471 * 1470) + (inOrder ? "" : ", NOT in order"));
		Check(chunks == 618 && mostFrames <= host::kFmvSoundMostFramesAChunk, "in " + std::to_string(chunks) + " chunks, none decoding more than " +
			std::to_string(host::kFmvSoundMostFramesAChunk) + " frames (the most: " + std::to_string(mostFrames) + ")");
		Check(stream.Done() && !stream.NextChunk(&chunk) && chunk.empty(), "then it is done: no more chunks");
		stream.Close();
		Check(opened == 1 && closed == 1, "its own handle opened once and closed once");
	}

	// A DECODER THAT DISAGREES WITH THE HEADER, OR NO SOUND: refused with the words the status line shows, nothing decoded, nothing left open
	{
		Reset();
		details.sampleRate = 11025;

		host::FmvSoundStream stream;
		std::string words;

		did = stream.Open(&calls, "message1.smk", HisFilm(), &words);
		Check(!did && words == "picture only: the decoder reads its sound differently from its header" && opened == closed &&
			framesDecoded == 0, "a decoder reading 11025 Hz where the header says 22050: refused, closed again (\"" + words + "\")");

		Reset();
		details.idealBufferSize = host::kFmvSoundMostFrameBuffer + 1;
		did = stream.Open(&calls, "message1.smk", HisFilm(), &words);
		Check(!did && opened == closed, "a frame's buffer over 16 MB: refused");

		Reset();
		details.bitsPerSample = 12;

		model::SmkHeader twelve = HisFilm();

		twelve.tracks[0].bits = 12;
		did = stream.Open(&calls, "message1.smk", twelve, &words);
		Check(!did && opened == closed, "12-bit sound, even when the header says so too: refused (8 or 16 only)");

		Reset();

		model::SmkHeader silent = HisFilm();

		silent.tracks[0].present = false;
		did = stream.Open(&calls, "message1.smk", silent, &words);
		Check(!did && words == "it has no sound" && opened == 0, "a film with no first track: \"it has no sound\", the decoder never opened");

		Reset();
		openWorks = false;
		did = stream.Open(&calls, "message1.smk", HisFilm(), &words);
		Check(!did && words == "picture only: the decoder would not open its sound", "a decoder that won't open it: \"" + words + "\"");

		Reset();
		tracks = 0;
		did = stream.Open(&calls, "message1.smk", HisFilm(), &words);
		Check(!did && opened == closed, "a decoder that finds no track: refused");

		did = stream.Open(nullptr, "message1.smk", HisFilm(), &words);
		Check(!did, "no decoder: refused");
	}

	// A FRAME THAT SAYS MORE THAN ITS BUFFER: only the buffer is read
	{
		Reset();
		bytesAFrame = 5000;

		host::FmvSoundStream stream;
		std::string words;
		std::vector<uint8_t> chunk;

		stream.Open(&calls, "message1.smk", HisFilm(), &words);
		stream.NextChunk(&chunk);
		Check(chunk.size() == 3 * 2048 && framesDecoded == 3, "a frame saying 5,000 bytes with a 2,048-byte buffer gives 2,048: a chunk of 3 frames, " + std::to_string(chunk.size()) +
			" bytes");
	}

	// 16-BIT STEREO WITH A ROUGH FRAME SIZE: every chunk whole sample frames (4 bytes), the bytes cut at a chunk's end carried to the next one
	{
		Reset();
		bytesAFrame = 2941;
		details = { 44100, 2, 16, 4096 };

		model::SmkHeader stereo = HisFilm();

		stereo.frames = 300;
		stereo.tracks[0].bits = 16;
		stereo.tracks[0].channels = 2;
		stereo.tracks[0].rate = 44100;
		filmFrames = 300;

		host::FmvSoundStream stream;
		std::string words;
		std::vector<uint8_t> chunk;
		size_t total = 0;
		bool whole = true;

		stream.Open(&calls, "stereo.smk", stereo, &words);

		while (stream.NextChunk(&chunk))
		{
			whole = whole && chunk.size() % 4 == 0;
			total += chunk.size();
		}

		Check(whole && total == (size_t)300 * 2941 / 4 * 4, "16-bit stereo, 2,941 bytes a frame: every chunk whole 4-byte frames, " + std::to_string(total) + " bytes in all (every byte: 300 x 2,941 is whole frames, the cut ones carried to the next chunk)");
	}

	// FRAMES WITH NO SOUND: chunks come empty until the film ends, never a stuck loop
	{
		Reset();
		bytesAFrame = 0;
		filmFrames = 100;

		model::SmkHeader quiet = HisFilm();

		quiet.frames = 100;

		host::FmvSoundStream stream;
		std::string words;
		std::vector<uint8_t> chunk;
		size_t calls_ = 0;

		stream.Open(&calls, "quiet.smk", quiet, &words);

		while (stream.NextChunk(&chunk) && calls_ < 1000)
		{
			calls_++;
		}

		Check(calls_ == 6 && framesDecoded == 100 && stream.Done(), "100 frames with no sound: 6 empty chunks of 16 frames, and the 7th, reading the last 4, ends it (" +
			std::to_string(calls_) + " came)");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return checksFailed ? 1 : 0;
}
