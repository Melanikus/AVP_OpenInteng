// shape_pane_store.cpp: THE LIVE PANES' STORE, ITS D3D PART (shape_pane_store.h; shape_pane_gpu.h's ShapePaneGpu_Upload, _Forget and _ForgetAll). Written
// 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng: Bishop's split of the store at Frost's seam, its pure part Vasquez's (shape_pane_pack.h), this the card's.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each listed shape's drawing is put on the card once a sitting (the owner: "The shapes don't change either (maps or shapes) they should live in the GPU"), and
// kept by a handle until the picker lets it go. An upload, in order, nothing made before every check has passed:
//   1. its triangles packed and every number checked (shape_pane_pack.h ShapePane_Pack: a stranger's shape is hostile input);
//   2. the pictures its batches use read as the picker reads them (shape_picker_screen.h ShapePickerScreen_ReadPicture: the judged path, the visit's
//      allowance, the sitting's kept pictures, so no file is read twice); one that can't be read is drawn shaded, as the still pictures are;
//   3. its bytes on the card (both buffers and every picture) counted against kShapePaneStoreMostBytes (ShapePane_RoomFor): past it, refused in words;
//   4. its buffers and pictures made in Direct3D's MANAGED pool (the card's copy Direct3D's own to keep across a reset: nothing to let go before one), each
//      picture's rows copied by the pitch its lock gives, never by its width. Any Create or Lock that fails lets go of everything this upload made, and says so.
// Handles are never given twice in a sitting, so a handle let go can't come to mean another drawing. Everything here is the main thread's, as the device is.
#include "3dc.h"
#include "inline.h"
#include "gameplat.h"
#include "platform.h"
#include "renderer.h"
#include "d3_func.h"

#include <windows.h>
#include <limits.h>
#include <string.h>

#include <map>
#include <new>
#include <string>
#include <vector>

#include "shape_pane_gpu.h"
#include "shape_pane_pack.h"
#include "shape_pane_store.h"
#include "shape_picker_screen.h"   // ShapePickerScreen_ReadPicture
#include "shape_thumb_render.h"    // ShapeThumbTexture

namespace
{
	struct Held
	{
		host::ShapePaneDrawing drawing = {};
		std::vector<IDirect3DTexture9 *> pictures;   // by the model's picture number; null where it was drawn shaded
		uint64_t bytes = 0;                          // what it takes on the card (counted against the most)
	};

	std::map<int, Held> held;      // (a map: a drawing's address stays the same while it is held, as ShapePaneStore_Find promises)
	uint64_t heldBytes = 0;
	int nextHandle = 0;

	template <typename T> void LetGo(T *&thing)
	{
		if (thing)
		{
			thing->Release();
			thing = nullptr;
		}
	}

	void LetGoOf(Held &h)
	{
		LetGo(h.drawing.corners);
		LetGo(h.drawing.indices);

		for (IDirect3DTexture9 *&picture : h.pictures)
		{
			LetGo(picture);
		}

		h.drawing.batches.clear();
	}

	// one picture on the card: MANAGED, A8R8G8B8, its rows copied by the pitch the lock gives; false (nothing kept) when it can't be made
	bool MakePicture(IDirect3DDevice9 *device, host::ShapeThumbTexture const &read, IDirect3DTexture9 **made)
	{
		*made = nullptr;

		if (read.width == 0 || read.height == 0 || read.bgra.size() != (size_t)read.width * (size_t)read.height * 4)
		{
			return false;
		}

		IDirect3DTexture9 *texture = nullptr;
		D3DLOCKED_RECT locked = {};

		if (FAILED(device->CreateTexture(read.width, read.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL)) || !texture)
		{
			return false;
		}

		if (FAILED(texture->LockRect(0, &locked, NULL, 0)) || !locked.pBits || locked.Pitch < (INT)(read.width * 4))
		{
			texture->Release();
			return false;
		}

		for (uint32_t row = 0; row < read.height; row++)
		{
			memcpy((uint8_t *)locked.pBits + (size_t)row * (size_t)locked.Pitch, read.bgra.data() + (size_t)row * read.width * 4, (size_t)read.width * 4);
		}

		texture->UnlockRect(0);
		*made = texture;
		return true;
	}

	// one buffer filled with bytes it was made the size of; false when the lock fails
	template <typename Buffer> bool Fill(Buffer *buffer, void const *bytes, UINT size)
	{
		void *data = nullptr;

		if (FAILED(buffer->Lock(0, size, &data, 0)) || !data)
		{
			return false;
		}

		memcpy(data, bytes, size);
		buffer->Unlock();
		return true;
	}
}

namespace host
{
	int ShapePaneGpu_Upload(ShapePaneModel const &model, std::string *why)
	{
		std::string notSaid;

		why = why ? why : &notSaid;
		why->clear();

		IDirect3DDevice9 *const device = d3d.lpD3DDevice;

		if (!device)
		{
			*why = "the live pane: the card isn't there";
			return -1;
		}

		if (nextHandle == INT_MAX)
		{
			*why = "the live pane: the picker has put as many drawings on the card as it may this sitting";
			return -1;
		}

		// 1. packed, every number checked
		ShapePanePacked packed;

		if (!ShapePane_Pack(model.batches, model.pictures.size(), &packed, why))
		{
			return -1;
		}

		D3DCAPS9 caps = {};

		if (FAILED(device->GetDeviceCaps(&caps)) || packed.corners.empty() || caps.MaxVertexIndex < packed.corners.size() - 1)
		{
			*why = "the live pane: this card can't number " + std::to_string((unsigned long long)packed.corners.size()) + " corners in one drawing";
			return -1;
		}

		// 2. the pictures the batches use, read as the picker reads them; one that can't be read is drawn shaded
		std::vector<ShapeThumbTexture> read(model.pictures.size());
		std::vector<bool> used(model.pictures.size(), false);
		uint64_t pictureBytes = 0, tileBytes = 0;

		for (ShapePanePackedBatch const &batch : packed.batches)
		{
			if (batch.picture >= 0)
			{
				used[(size_t)batch.picture] = true;   // (Pack checked it is below the pictures there are)
			}
		}

		for (size_t p = 0; p < model.pictures.size(); p++)
		{
			std::string pictureWhy;

			if (used[p] && !ShapePickerScreen_ReadPicture(model.pictures[p], &read[p], &pictureWhy, &tileBytes))
			{
				read[p] = ShapeThumbTexture();
			}

			pictureBytes += read[p].bgra.size();
		}

		// 3. its room on the card
		if (!ShapePane_RoomFor(heldBytes, packed.bufferBytes + pictureBytes, why))
		{
			return -1;
		}

		// 4. made, in the MANAGED pool; any failure lets go of everything made so far
		Held h;
		UINT const cornerBytes = (UINT)(packed.corners.size() * sizeof(ShapePaneCorner));
		UINT const indexBytes = (UINT)(packed.indices.size() * sizeof(uint32_t));

		h.pictures.assign(model.pictures.size(), nullptr);

		if (FAILED(device->CreateVertexBuffer(cornerBytes, D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1, D3DPOOL_MANAGED, &h.drawing.corners, NULL)) ||
			!h.drawing.corners || !Fill(h.drawing.corners, packed.corners.data(), cornerBytes))
		{
			LetGoOf(h);
			*why = "the live pane: the card can't make its corners (" + std::to_string((unsigned long long)cornerBytes) + " bytes)";
			return -1;
		}

		if (FAILED(device->CreateIndexBuffer(indexBytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED, &h.drawing.indices, NULL)) || !h.drawing.indices ||
			!Fill(h.drawing.indices, packed.indices.data(), indexBytes))
		{
			LetGoOf(h);
			*why = "the live pane: the card can't make its triangles (" + std::to_string((unsigned long long)indexBytes) + " bytes)";
			return -1;
		}

		for (size_t p = 0; p < read.size(); p++)
		{
			if (!read[p].bgra.empty() && !MakePicture(device, read[p], &h.pictures[p]))
			{
				LetGoOf(h);
				*why = "the live pane: the card can't make its picture " + std::to_string((unsigned long long)p + 1) + " (" + std::to_string(read[p].width) + " x " +
					std::to_string(read[p].height) + ")";
				return -1;
			}
		}

		for (ShapePanePackedBatch const &batch : packed.batches)
		{
			ShapePaneBatch b = {};

			b.picture = batch.picture >= 0 ? h.pictures[(size_t)batch.picture] : nullptr;   // (null: its picture couldn't be read, drawn shaded)
			b.firstIndex = batch.firstIndex;
			b.triangles = batch.triangles;
			b.minVertex = batch.minVertex;
			b.vertexCount = batch.vertexCount;
			h.drawing.batches.push_back(b);
		}

		h.drawing.triangles = packed.triangles;
		memcpy(h.drawing.eye, model.eye, sizeof(h.drawing.eye));
		memcpy(h.drawing.at, model.at, sizeof(h.drawing.at));
		h.bytes = packed.bufferBytes + pictureBytes;

		int const handle = nextHandle;

		try
		{
			held[handle] = h;
		}
		catch (std::bad_alloc const &)
		{
			held.erase(handle);
			LetGoOf(h);
			*why = "the live pane: not enough memory to keep it";
			return -1;
		}

		nextHandle++;
		heldBytes += h.bytes;
		return handle;
	}

	void ShapePaneGpu_Forget(int handle)
	{
		auto const it = held.find(handle);

		if (it == held.end())
		{
			return;
		}

		heldBytes -= it->second.bytes;
		LetGoOf(it->second);
		held.erase(it);
	}

	void ShapePaneGpu_ForgetAll()
	{
		for (auto &each : held)
		{
			LetGoOf(each.second);
		}

		held.clear();
		heldBytes = 0;
	}

	ShapePaneDrawing const *ShapePaneStore_Find(int handle)
	{
		auto const it = held.find(handle);

		return it == held.end() ? nullptr : &it->second.drawing;
	}
}
