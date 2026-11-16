// edge_media.cpp: the editor's stand-ins for the film decoders (Bink, Smacker) and the music libraries (vorbisfile, pthreads).
// Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng (R-INT-15).
//
// WHY: the editor compiles the game's own film and music players (avpx/3dc/video/BinkPlayback.cpp and SmackerPlayback.cpp,
// avpx/3dc/VorbisPlayer.cpp, avpx/3dc/avp/win95/openal_stream.cpp), because the rest of the game calls into them. But the editor never
// plays a film or a piece of music, so it doesn't link the libraries that decode them: none of their code ever runs on a file in the
// editor. These stand-ins take the libraries' place, and each one does one of two things:
//   - QUIET: a call the game's players make on every file, even one that won't open, answers exactly what the real library answers for
//     a file that won't open. The game's players already have a "can't open" path, for a missing file, and they take it. These are the
//     opens, and the closes the players' destructors call either way.
//   - LOUD: every other call can only come after an open worked, which never happens here. Each of those stops the editor at once with
//     its own name (EdgeStop), so if a later AvPx ever reaches one some other way, nobody can miss it.
//
// tools/make-edge.js checks, at every pin move, that this file defines exactly the names the linker can't find among these libraries:
// one missing here fails it, and so does one here that the game no longer calls.

#include "BinkDecoder.h"
#include "SmackerDecoder.h"
#include <vorbis/vorbisfile.h>
#include <pthread.h>
#include <string.h>
#include "edge_stop.h"

// Bink (avpx/dependencies/libbinkdec)

// QUIET. The real one starts its handle as { not valid, place -1 } and hands back exactly that when the file doesn't open
// (libbinkdec/src/BinkDecoder.cpp:86-88). BinkPlayback::Open stops at once on it (avpx/3dc/video/BinkPlayback.cpp:73-77).
BinkHandle Bink_Open(const char *)
{
	BinkHandle handle;

	handle.isValid = false;
	handle.instanceIndex = -1;
	return handle;
}

// QUIET. The real one does nothing at all for a handle that never opened (BinkDecoder.cpp:135-141), and BinkPlayback's destructor
// calls it even then (BinkPlayback.cpp:246).
void Bink_Close(BinkHandle &)
{
}

uint32_t Bink_GetNumAudioTracks(BinkHandle &)
{
	EdgeStop("Bink_GetNumAudioTracks");
}

void Bink_GetFrameSize(BinkHandle &, uint32_t &, uint32_t &)
{
	EdgeStop("Bink_GetFrameSize");
}

AudioInfo Bink_GetAudioTrackDetails(BinkHandle &, uint32_t)
{
	EdgeStop("Bink_GetAudioTrackDetails");
}

uint32_t Bink_GetAudioData(BinkHandle &, uint32_t, int16_t *)
{
	EdgeStop("Bink_GetAudioData");
}

float Bink_GetFrameRate(BinkHandle &)
{
	EdgeStop("Bink_GetFrameRate");
}

uint32_t Bink_GetNumFrames(BinkHandle &)
{
	EdgeStop("Bink_GetNumFrames");
}

uint32_t Bink_GetCurrentFrameNum(BinkHandle &)
{
	EdgeStop("Bink_GetCurrentFrameNum");
}

uint32_t Bink_GetNextFrame(BinkHandle &, YUVbuffer)
{
	EdgeStop("Bink_GetNextFrame");
}

void Bink_GotoFrame(BinkHandle &, uint32_t)
{
	EdgeStop("Bink_GotoFrame");
}

// Smacker (avpx/dependencies/libsmackerdec)

// QUIET. As Bink_Open: the real one's handle is { not valid, place -1 } when the file doesn't open (libsmackerdec/src/SmackerDecoder.cpp:
// 91-93), and SmackerPlayback::Open stops at once on it (avpx/3dc/video/SmackerPlayback.cpp:96-101).
SmackerHandle Smacker_Open(const char *)
{
	SmackerHandle handle;

	handle.isValid = false;
	handle.instanceIndex = -1;
	return handle;
}

// QUIET. As Bink_Close: nothing at all for a handle that never opened (SmackerDecoder.cpp:138-144), called by SmackerPlayback's
// destructor either way (SmackerPlayback.cpp:81).
void Smacker_Close(SmackerHandle &)
{
}

uint32_t Smacker_GetNumAudioTracks(SmackerHandle &)
{
	EdgeStop("Smacker_GetNumAudioTracks");
}

SmackerAudioInfo Smacker_GetAudioTrackDetails(SmackerHandle &, uint32_t)
{
	EdgeStop("Smacker_GetAudioTrackDetails");
}

uint32_t Smacker_GetAudioData(SmackerHandle &, uint32_t, int16_t *)
{
	EdgeStop("Smacker_GetAudioData");
}

uint32_t Smacker_GetNumFrames(SmackerHandle &)
{
	EdgeStop("Smacker_GetNumFrames");
}

void Smacker_GetFrameSize(SmackerHandle &, uint32_t &, uint32_t &)
{
	EdgeStop("Smacker_GetFrameSize");
}

uint32_t Smacker_GetNextFrame(SmackerHandle &)
{
	EdgeStop("Smacker_GetNextFrame");
}

float Smacker_GetFrameRate(SmackerHandle &)
{
	EdgeStop("Smacker_GetFrameRate");
}

void Smacker_GetPalette(SmackerHandle &, uint8_t *)
{
	EdgeStop("Smacker_GetPalette");
}

void Smacker_GetFrame(SmackerHandle &, uint8_t *)
{
	EdgeStop("Smacker_GetFrame");
}

// vorbisfile (libvorbis). These two keep the C names vorbisfile.h gives them, inside its own extern "C".

// QUIET. When a file isn't Ogg Vorbis, the real one empties the whole OggVorbis_File (libvorbis's vorbisfile.c: _ov_open1 sets its
// datasource to NULL and calls ov_clear, which ends with a memset of the whole struct) and answers below 0. This one does the same
// for every file, without reading a byte of it. VorbisPlayback::Open closes the file and stops on any answer below 0
// (avpx/3dc/VorbisPlayer.cpp:89-95).
int ov_open_callbacks(void *, OggVorbis_File *vf, const char *, long, ov_callbacks)
{
	if (vf)
	{
		memset(vf, 0, sizeof(*vf));
	}
	return OV_ENOTVORBIS;
}

// QUIET. VorbisPlayback's destructor calls it for every player (VorbisPlayer.cpp:153), and here the struct it gets is always an empty
// one: the player's constructor empties it (avpx/3dc/VorbisPlayer.h:86), and so does a failed open. The real one, given an empty struct,
// has nothing to free and no file to close (its datasource is NULL), empties it again and answers 0.
int ov_clear(OggVorbis_File *vf)
{
	if (vf)
	{
		memset(vf, 0, sizeof(*vf));
	}
	return 0;
}

// ov_info and ov_read are only called after an open worked (VorbisPlayer.cpp:98, :166).
vorbis_info *ov_info(OggVorbis_File *, int)
{
	EdgeStop("ov_info");
}

long ov_read(OggVorbis_File *, char *, int, int, int, int, int *)
{
	EdgeStop("ov_read");
}

// pthreads (pthreads4w). The game's only thread user here is its streamed sound (avpx/3dc/avp/win95/openal_stream.cpp), and a stream is
// only made after a film or a piece of music opened (BinkPlayback.cpp:103, SmackerPlayback.cpp:120, VorbisPlayer.cpp after :98), which
// never happens here. So every one of them is LOUD. They keep pthread.h's own C names and its __cdecl.

int __cdecl pthread_create(pthread_t *, const pthread_attr_t *, void *(__cdecl *)(void *), void *)
{
	EdgeStop("pthread_create");
}

int __cdecl pthread_join(pthread_t, void **)
{
	EdgeStop("pthread_join");
}

int __cdecl pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *)
{
	EdgeStop("pthread_mutex_init");
}

int __cdecl pthread_mutex_destroy(pthread_mutex_t *)
{
	EdgeStop("pthread_mutex_destroy");
}

int __cdecl pthread_mutex_lock(pthread_mutex_t *)
{
	EdgeStop("pthread_mutex_lock");
}

int __cdecl pthread_mutex_unlock(pthread_mutex_t *)
{
	EdgeStop("pthread_mutex_unlock");
}
