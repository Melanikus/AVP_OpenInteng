// See fmv_preview.h. Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "3dc.h"
#include "module.h"         // MODULE, which d3d_render.h needs
#include "inline.h"
#include "renderer.h"
#include "renderStates.h"   // TRANSLUCENCY_OFF
#include "d3_func.h"
#include "d3d_render.h"
#include "TextureManager.h" // Tex_Create / Tex_Lock: the picture's texture; Tex_GetNamesVector / Tex_GetDimensions: the level's pictures
#include "logString.h"

#include <windows.h>

#include <AL\al.h>

#include <string.h>

#include <string>
#include <vector>

#include "fmv_files.h"
#include "fmv_preview.h"
#include "fmv_sound_stream.h"
#include "smacker_dll.h"
#include "smk_header.h"
#include "state_line.h"
#include "text_scale.h"     // the editor's own pixels to the window's

extern int SoundSwitchedOn;         // avp/psnd.cpp: 1 once the game's sound system started (there is a sound device)
extern void SoundSys_Start(void);   // avp/psnd.cpp: it makes the OpenAL device and context the preview's sound plays in
extern bool editorHiddenRun;        // a hidden run (the harness's): every sound silent

namespace host
{
	namespace
	{
		bool on = false;

		// THE PREVIEW'S SOUND, STREAMED (Vasquez 2026-11-21, the owner's "her video is long so it got stuck for a few seconds, we need a loading video %"; fmv_sound_stream.h): the
		// first track read a chunk at a time and queued on one OpenAL source in a ring of kFmvSoundChunks buffers, each refilled once it has been played (FeedSound, every editor
		// frame). Play starts at once whatever the film's length, so there is no load left to show a % for. Before, the whole track was read into one buffer first (2,471 frames
		// decoded for his 164.7 s film, the editor stuck for seconds); the 20-minute and 64 MB limits that one buffer needed are gone with it.
		FmvSoundStream soundStream;
		ALuint soundSource = 0;
		ALuint soundBuffers[kFmvSoundChunks] = {};
		std::vector<ALuint> freeBuffers;   // the ring's buffers not on the source now
		ALenum soundFormat = AL_FORMAT_MONO8;
		std::vector<uint8_t> chunk;

		// the preview's own frame times, for the harness (fmvpreview): how long Play's start took, and the longest gap between two of its editor frames since
		ULONGLONG startedInMs = 0, longestFrameMs = 0, lastDrawnAt = 0;
		std::string startedSteps;   // the start's steps, each in ms: "stop 0, header 0, decoder 15, picture 0, sound 16"

		void StopSound()
		{
			if (soundSource)
			{
				alSourceStop(soundSource);
				alSourcei(soundSource, AL_BUFFER, 0);   // a stopped source lets go of every buffer queued on it
				alDeleteSources(1, &soundSource);
				soundSource = 0;
			}

			if (soundBuffers[0])
			{
				alDeleteBuffers((ALsizei)kFmvSoundChunks, soundBuffers);

				for (ALuint &b : soundBuffers)
				{
					b = 0;
				}
			}

			freeBuffers.clear();
			soundStream.Close();
		}

		// The next chunk onto the source in a free buffer of the ring; false when no buffer is free or the track is read to its end
		bool QueueChunk()
		{
			if (freeBuffers.empty() || !soundStream.NextChunk(&chunk))
			{
				return false;
			}

			if (chunk.empty())
			{
				return true;   // frames with no sound in them: nothing to queue this time
			}

			ALuint const buffer = freeBuffers.back();

			alGetError();
			alBufferData(buffer, soundFormat, chunk.data(), (ALsizei)chunk.size(), (ALsizei)soundStream.Rate());
			alSourceQueueBuffers(soundSource, 1, &buffer);

			if (alGetError() != AL_NO_ERROR)
			{
				return false;
			}

			freeBuffers.pop_back();
			return true;
		}

		// Every editor frame while it plays: the buffers played are taken back and refilled with the next chunks (a couple at most), and a source that ran dry while more was
		// coming (the editor held up longer than the sound queued) plays on
		void FeedSound()
		{
			if (!soundSource)
			{
				return;
			}

			ALint played = 0;

			alGetSourcei(soundSource, AL_BUFFERS_PROCESSED, &played);

			for (; played > 0; played--)
			{
				ALuint buffer = 0;

				alSourceUnqueueBuffers(soundSource, 1, &buffer);
				freeBuffers.push_back(buffer);
			}

			for (uint32_t refill = 0; refill < kFmvSoundMostRefillsAFrame && QueueChunk(); refill++)
			{
			}

			ALint state = AL_STOPPED, queued = 0;

			alGetSourcei(soundSource, AL_SOURCE_STATE, &state);
			alGetSourcei(soundSource, AL_BUFFERS_QUEUED, &queued);

			if (state != AL_PLAYING && queued > 0)
			{
				alSourcePlay(soundSource);
			}
		}

		// The first track opened as a stream, the ring made and its first second queued (the picture and the sound start together after it); the words for what plays:
		// "with its sound", or why not
		std::string soundSteps;   // StartSound's own steps, each in ms (the start's "sound" share, split)

		std::string StartSound(SmackerDllCalls const *dll, std::string const &path, model::SmkHeader const &header)
		{
			ULONGLONG mark = GetTickCount64();
			auto const lap = [&mark](char const *step)
			{
				ULONGLONG const now = GetTickCount64();

				soundSteps += std::string(soundSteps.empty() ? "" : " ") + step + " " + std::to_string((unsigned long long)(now - mark));
				mark = now;
			};

			soundSteps.clear();
			if (!header.tracks[0].present)
			{
				return "it has no sound";
			}

			if (!SoundSwitchedOn)
			{
				SoundSys_Start();
				lap("device");
			}

			if (!SoundSwitchedOn)
			{
				return "picture only: no sound device";
			}

			std::string words;

			bool const streamOpened = soundStream.Open(dll, path, header, &words);

			lap("open");

			if (!streamOpened)
			{
				return words;
			}

			uint32_t const channels = soundStream.Channels(), bits = soundStream.Bits();

			soundFormat = channels == 1 ? (bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16) : (bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16);
			alGetError();
			alGenBuffers((ALsizei)kFmvSoundChunks, soundBuffers);
			alGenSources(1, &soundSource);
			alSourcei(soundSource, AL_SOURCE_RELATIVE, AL_TRUE);
			alSource3f(soundSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
			alSourcef(soundSource, AL_GAIN, editorHiddenRun ? 0.0f : 1.0f);

			if (alGetError() != AL_NO_ERROR)
			{
				StopSound();
				return "picture only: the sound device refused its sound";
			}

			lap("ring");
			freeBuffers.assign(soundBuffers, soundBuffers + kFmvSoundChunks);

			for (uint32_t first = 0; first < kFmvSoundStartChunks && QueueChunk(); first++)
			{
			}

			lap("first-second");

			return words;
		}
		SmackerDllCalls const *calls = nullptr;
		SmackerHandle video = { false, -1 };
		std::string shownName;
		uint32_t width = 0, height = 0, frames = 0;
		double msPerFrame = 100.0;
		ULONGLONG startedAt = 0;
		uint32_t decoded = 0;       // frames decoded so far (the next GetNextFrame gives frame `decoded`)
		std::vector<uint8_t> palette, pixels;
		FmvPreviewPlace place;
		texID_t picture = MISSING_TEXTURE;
		uint32_t pictureWidth = 0, pictureHeight = 0;
		bool pictureFilled = false;

		void ReleasePicture()
		{
			if (picture != MISSING_TEXTURE)
			{
				Tex_Release(picture);
				picture = MISSING_TEXTURE;
			}

			pictureWidth = pictureHeight = 0;
			pictureFilled = false;
		}

		// the decoder's frame (256 colours, R G B, and a byte a pixel) into the texture, as the game's own ConvertFrame does (SmackerPlayback.cpp: B, G, R, 255 a pixel)
		bool FillPicture()
		{
			if (picture == MISSING_TEXTURE || pictureWidth != width || pictureHeight != height)
			{
				ReleasePicture();
				picture = Tex_Create("editor_fmv_preview", width, height, 32, TextureUsage_Normal);

				if (picture == MISSING_TEXTURE)
				{
					return false;
				}

				pictureWidth = width;
				pictureHeight = height;
			}

			uint8_t *data = nullptr;
			uint32_t pitch = 0;

			if (!Tex_Lock(picture, &data, &pitch) || !data || pitch < width * 4)
			{
				return false;
			}

			for (uint32_t y = 0; y < height; y++)
			{
				uint8_t *to = data + (size_t)y * pitch;
				uint8_t const *from = pixels.data() + (size_t)y * width;

				for (uint32_t x = 0; x < width; x++)
				{
					uint32_t const at = (uint32_t)from[x] * 3;

					*to++ = palette[at + 2];
					*to++ = palette[at + 1];
					*to++ = palette[at + 0];
					*to++ = 255;
				}
			}

			Tex_Unlock(picture);
			pictureFilled = true;
			return true;
		}

		void Fit(int32_t x, int32_t y, int32_t w, int32_t h, int32_t *outX, int32_t *outY, int32_t *outW, int32_t *outH)
		{
			// the picture's shape kept, as large as fits, centred
			double const scale = (double)w / width < (double)h / height ? (double)w / width : (double)h / height;

			*outW = (int32_t)(width * scale);
			*outH = (int32_t)(height * scale);
			*outX = x + (w - *outW) / 2;
			*outY = y + (h - *outH) / 2;
		}
	}

	uint32_t FmvPreview_DueFrame(double elapsedMs, double msPerFrame, uint32_t frames)
	{
		if (frames == 0 || !(msPerFrame > 0.0) || !(elapsedMs > 0.0))
		{
			return 0;
		}

		double const due = elapsedMs / msPerFrame;

		return due >= (double)(frames - 1) ? frames - 1 : (uint32_t)due;
	}

	bool FmvPreview_Start(std::string const &folder, std::string const &name, char const *settingsFile, FmvPreviewPlace const &where, std::string *words)
	{
		ULONGLONG const asked = GetTickCount64();   // how long the start takes, for the harness (fmvpreview "started in")
		ULONGLONG mark = asked;
		std::string steps;   // each step's share of it: where a slow start goes
		auto const lap = [&mark, &steps](char const *step)
		{
			ULONGLONG const now = GetTickCount64();

			steps += (steps.empty() ? "" : ", ") + std::string(step) + " " + std::to_string((unsigned long long)(now - mark));
			mark = now;
		};

		FmvPreview_Stop();
		lap("stop");

		std::string const shown = "FMVs\\" + name;
		model::SmkHeader header;
		std::string why;

		switch (FmvFiles_Header(folder, name, &header, &why))
		{
		case FmvFileRead::Read:
			break;
		case FmvFileRead::Missing:
			*words = "No " + shown + " to play: copy it into the game folder's FMVs";
			return false;
		default:
			*words = "Not played: " + shown + " is not a Smacker video the game can play";
			return false;
		}

		lap("header");

		if (header.width > (uint32_t)kFmvPreviewMostSide || header.height > (uint32_t)kFmvPreviewMostSide)
		{
			*words = "Not played: " + shown + " is " + std::to_string(header.width) + " x " + std::to_string(header.height) + ", larger than the preview shows";
			return false;
		}

		std::string note;
		SmackerDllCalls const *const got = SmackerDll_Get(settingsFile, &why, &note);

		lap("decoder");

		if (!got)
		{
			*words = why;
			return false;
		}

		std::string root = folder;

		while (!root.empty() && (root.back() == '\\' || root.back() == '/'))
		{
			root.pop_back();
		}

		SmackerHandle opened = got->Open((root + "\\" + name).c_str());

		lap("picture");

		if (!opened.isValid)
		{
			*words = "Not played: the decoder would not open " + shown;
			return false;
		}

		uint32_t w = 0, h = 0;

		got->GetFrameSize(opened, w, h);

		uint32_t const n = got->GetNumFrames(opened);

		// the decoder has to agree with the header we checked
		if (w != header.width || h != header.height || n != header.frames || n == 0)
		{
			got->Close(opened);
			*words = "Not played: the decoder reads " + shown + " differently from its header";
			return false;
		}

		std::string const soundWords = StartSound(got, root + "\\" + name, header);

		lap("sound");
		steps += " [" + soundSteps + "]";

		calls = got;
		video = opened;
		shownName = name;
		width = w;
		height = h;
		frames = n;
		msPerFrame = header.msPerFrame;
		palette.assign(768, 0);
		pixels.assign((size_t)w * h, 0);
		decoded = 0;
		place = where;
		startedAt = GetTickCount64();
		startedInMs = startedAt - asked;
		startedSteps = steps;
		longestFrameMs = 0;
		lastDrawnAt = startedAt;
		on = true;

		if (soundSource)
		{
			alSourcePlay(soundSource);
		}

		*words = "Playing " + shown + ": " + model::SmkHeader_Words(header) + " (" + note + "; " + soundWords + ")";
		return true;
	}

	void FmvPreview_Stop()
	{
		if (on && calls)
		{
			calls->Close(video);
		}

		StopSound();
		on = false;
		video = { false, -1 };
		place = nullptr;
		pictureFilled = false;
	}

	bool FmvPreview_On()
	{
		return on;
	}

	void FmvPreview_Draw(int32_t viewWidth, int32_t viewHeight)
	{
		if (!on)
		{
			StateLine_Add("fmvpreview", "");
			return;
		}

		int32_t x = 0, y = 0, w = 0, h = 0;

		if (!place || !place(&x, &y, &w, &h) || w < 1 || h < 1 || viewWidth < 1 || viewHeight < 1)
		{
			FmvPreview_Stop();
			StateLine_Add("fmvpreview", "");
			return;
		}

		// the gap since its last editor frame (since the start, for the first): the longest one is said in fmvpreview
		ULONGLONG const now = GetTickCount64();

		longestFrameMs = now - lastDrawnAt > longestFrameMs ? now - lastDrawnAt : longestFrameMs;
		lastDrawnAt = now;

		// the sound: the buffers played refilled with the next chunks (fmv_sound_stream.h)
		FeedSound();

		// the frames due by now, a few at most this editor frame
		uint32_t const due = FmvPreview_DueFrame((double)(GetTickCount64() - startedAt), msPerFrame, frames);

		for (int32_t step = 0; step < kFmvPreviewMostDecodesAFrame && (decoded == 0 || decoded <= due) && decoded < frames; step++)
		{
			calls->GetNextFrame(video);
			decoded++;

			if (decoded > due || decoded == frames || step == kFmvPreviewMostDecodesAFrame - 1)
			{
				calls->GetPalette(video, palette.data());
				calls->GetFrame(video, pixels.data());
				FillPicture();
			}
		}

		ALint sounding = AL_STOPPED;

		if (soundSource)
		{
			alGetSourcei(soundSource, AL_SOURCE_STATE, &sounding);
		}

		StateLine_Add("fmvpreview", shownName + " frame " + std::to_string((unsigned long long)(decoded == 0 ? 0 : decoded - 1)) + " of " + std::to_string((unsigned long long)frames) +
			(soundSource ? (sounding == AL_PLAYING ? " sound playing" : " sound ended") : " no sound") + "; started in " + std::to_string((unsigned long long)startedInMs) + " ms (" + startedSteps + "); longest frame " +
			std::to_string((unsigned long long)longestFrameMs) + " ms");

		if (!pictureFilled)
		{
			return;
		}

		int32_t fx = 0, fy = 0, fw = 0, fh = 0, wx = 0, wy = 0, ww = 0, wh = 0;

		Fit(x, y, w, h, &fx, &fy, &fw, &fh);
		TextScale_BoxToWindow(fx, fy, fw, fh, &wx, &wy, &ww, &wh);
		CheckWireFrameMode(0);
		DrawQuadSharp((uint32_t)wx, (uint32_t)wy, (uint32_t)ww, (uint32_t)wh, picture, 0xFFFFFFFFu, TRANSLUCENCY_OFF);
	}

	char const *FmvScreens_SayAtOpen(char const *gameFolder, char const *otherwise)
	{
		static std::string words;   // the status line points at it
		std::vector<std::string> names;
		std::string root = gameFolder ? gameFolder : "";

		while (!root.empty() && (root.back() == '\\' || root.back() == '/'))
		{
			root.pop_back();
		}

		Tex_GetNamesVector(names);

		// the game asks picture i's size by its place in the list (ScanImagesForFMVs's Tex_GetDimensions(i), its own TODO): the same here, so the verdict is the game's; i is inside
		// the list, which is never longer than the texture manager's own
		std::vector<host::FmvScreenRow> const rows = FmvFiles_JudgeScreens(names, [](size_t i, uint32_t *w, uint32_t *h)
		{
			uint32_t width = 0, height = 0;

			Tex_GetDimensions((texID_t)i, width, height);
			*w = width;
			*h = height;
		}, [&root](std::string const &smk)
		{
			if (root.empty() || root.size() + 1 + smk.size() >= MAX_PATH)
			{
				return false;
			}

			DWORD const attributes = GetFileAttributesA((root + "\\" + smk).c_str());

			return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);   // what the game's fopen would open
		});

		for (FmvScreenRow const &row : rows)
		{
			LogString("Video screen " + row.picture + ": " + (row.verdict == FmvScreen::Screen ? std::string("plays the level's messages") : (row.verdict == FmvScreen::StillItsSmkIsThere ?
				"stays still, " + row.smk + " is there" : (row.verdict == FmvScreen::StillWrongSize ? "stays still, " + std::to_string(row.width) + " x " + std::to_string(row.height) :
				std::string("stays still, past the tenth screen")))));
		}

		std::string const said = FmvFiles_ScreensWords(rows);

		if (said.empty())
		{
			return otherwise;
		}

		words = said + (otherwise && otherwise[0] ? std::string("; ") + otherwise : std::string());
		return words.c_str();
	}
}
