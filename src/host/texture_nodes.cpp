// texture_nodes.cpp: the Texture Map's cycle and reverse of the selected polygons' nodes (texture_nodes.h says what and why). Written 2026-11-18 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "texture_nodes.h"

#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "editor_shared.h"     // texturePolygons, textureWords, statusWords, TexturePolygon
#include "texture_shared.h"    // TextureUndo_RecordSelection, ShowTheNewUv
#include "level_objects.h"     // LevelObjects_OpenForEdit
#include "level_file.h"
#include "shape_edit.h"
#include "polygon_items.h"     // PolygonItems_ReadMergeData
#include "uv_edit.h"
#include "anim_edit.h"
#include "texture_anim.h"
#include "texture_quad.h"
#include "uv_model.h"
#include "uv_nodes.h"

namespace
{
	model::AnimPolygon *AnimOf(model::TextureAnim *anim, int32_t polygon)
	{
		for (model::AnimPolygon &one : anim->polygons)
		{
			if (one.polygon == polygon)
			{
				return &one;
			}
		}

		return nullptr;
	}

	// every frame of a polygon's animation turned as its nodes are: the count of frames turned
	size_t TurnFramesAlone(model::AnimPolygon *one, model::NodeTurn turn)
	{
		size_t frames = 0;

		for (model::AnimSequence &sequence : one->sequences)
		{
			for (size_t f = 0; f < sequence.Frames() && (f + 1) * 6 <= sequence.uv.size(); f++)
			{
				model::UvNodes_FrameAlone(&sequence.uv[f * 6], turn);
				frames++;
			}
		}

		return frames;
	}

	// a pair's frames together, when both animate in one group with the same counts (texture_nodes.h NAMED)
	size_t TurnFramesQuad(model::TextureQuad const &quad, model::AnimPolygon *own, model::AnimPolygon *partner, model::NodeTurn turn)
	{
		if (!own || !partner || own->group != partner->group || own->sequences.size() != partner->sequences.size())
		{
			return 0;
		}

		for (size_t s = 0; s < own->sequences.size(); s++)
		{
			if (own->sequences[s].Frames() != partner->sequences[s].Frames() || own->sequences[s].uv.size() != partner->sequences[s].uv.size())
			{
				return 0;
			}
		}

		size_t frames = 0;

		for (size_t s = 0; s < own->sequences.size(); s++)
		{
			for (size_t f = 0; f < own->sequences[s].Frames() && (f + 1) * 6 <= own->sequences[s].uv.size(); f++)
			{
				model::UvNodes_FrameQuad(quad, &own->sequences[s].uv[f * 6], &partner->sequences[s].uv[f * 6], turn);
				frames++;
			}
		}

		return frames;
	}
}

void TextureNodes_Command(int32_t command)
{
	bool const allFrames = command == 0xc0 || command == 0xc2;
	model::NodeTurn const turn = command == 0xbf || command == 0xc0 ? model::NodeTurn::Cycle : model::NodeTurn::Reverse;
	std::string const name = std::string(turn == model::NodeTurn::Cycle ? "Cycle nodes" : "Reverse nodes") + (allFrames ? " (every frame)" : "");
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level())
	{
		textureWords = "Texture Map: no level is open";
		statusWords = textureWords.c_str();
		return;
	}

	if (texturePolygons.empty())
	{
		textureWords = "Texture Map: nothing is selected";
		statusWords = textureWords.c_str();
		return;
	}

	TextureUndo_RecordSelection();   // FUN_00571310 0x57132e / FUN_00571910 0x57192e: FUN_00573570 first of all

	size_t changed = 0, notTextured = 0, left = 0, frames = 0;
	std::string firstWhy;
	std::vector<bool> done(texturePolygons.size(), false);
	std::vector<host::AnimWrite> animWrites;

	for (size_t first = 0; first < texturePolygons.size(); first++)
	{
		if (done[first])
		{
			continue;
		}

		TexturePolygon const lead = texturePolygons[first];
		std::vector<int32_t> selected;

		for (size_t i = first; i < texturePolygons.size(); i++)
		{
			TexturePolygon const &one = texturePolygons[i];

			if (!done[i] && one.isModule == lead.isModule && one.moduleIndex == lead.moduleIndex && memcmp(one.id, lead.id, 8) == 0)
			{
				done[i] = true;
				selected.push_back(one.polygon);
			}
		}

		model::ShapeModel shape;
		std::string why;
		std::vector<int32_t> merge;
		bool hasMerge = false;

		if (host::ShapeEdit_Read(open->Level(), open, lead.id, &shape, nullptr, &why) != host::ShapeEditResult::Done ||
			!host::PolygonItems_ReadMergeData(open, open->Level(), lead.id, &merge, &hasMerge))
		{
			left += selected.size();
			firstWhy = firstWhy.empty() ? "the shape could not be read" : firstWhy;
			continue;
		}

		std::map<int32_t, model::PolygonUv> written;   // by the file's polygon number: each written once
		model::TextureAnim anim;
		int32_t shapeNumber = 0;
		bool const animRead = allFrames && host::AnimEdit_Read(open, lead.id, &shapeNumber, &anim) == host::AnimEditResult::Done;
		size_t shapeFrames = 0;

		auto const uvOf = [&](int32_t polygon, model::PolygonUv *out) -> bool
		{
			auto const found = written.find(polygon);

			if (found != written.end())
			{
				*out = found->second;
				return true;
			}

			std::string readWhy;

			return host::UvEdit_ReadPolygon(open, open->Level(), lead.id, polygon, out, &readWhy) == host::UvEditResult::Done;
		};

		for (int32_t const p : selected)
		{
			model::PolygonUv own;

			if (!uvOf(p, &own))
			{
				left++;
				continue;
			}

			int const corners = own.cornerCount;

			if (!own.textured || (corners != 3 && corners != 4) || own.uv.cornerCount < corners)
			{
				notTextured++;
				continue;
			}

			int32_t const partner = corners == 3 && hasMerge && p >= 0 && (size_t)p < merge.size() ? merge[(size_t)p] : -1;
			model::TextureQuad quad = {};
			model::TextureQuadResult const quadResult = partner >= 0 ? model::TextureQuadOf(shape, p, partner, &quad) : model::TextureQuadResult::Alone;

			if (partner >= 0 && quadResult == model::TextureQuadResult::Ok)
			{
				if (p > partner)
				{
					continue;   // the pair turns once, from its lower number (0x5714ee / 0x571af6): the higher half selected alone does nothing
				}

				model::PolygonUv other;

				if (!uvOf(partner, &other) || !other.textured || other.cornerCount != 3 || other.uv.cornerCount < 3)
				{
					continue;   // the partner not textured: inteng turns nothing of the pair (FUN_0042bce0 of its picture, 0x571500)
				}

				model::UvNodes_Quad(quad, own.uv.corners, other.uv.corners, turn);
				written[p] = own;
				written[partner] = other;

				if (animRead)
				{
					shapeFrames += TurnFramesQuad(quad, AnimOf(&anim, p), AnimOf(&anim, partner), turn);
				}

				continue;
			}

			// alone (no partner, or one TextureQuadOf can't use: DECIDED UV-NODES-ALONE-PARTNER), and a file quad (DECIDED UV-NODES-FILE-QUAD)
			model::UvNodes_Alone(own.uv.corners, corners, turn);
			written[p] = own;

			model::AnimPolygon *const animated = animRead ? AnimOf(&anim, p) : nullptr;

			if (animated)
			{
				shapeFrames += TurnFramesAlone(animated, turn);
			}
		}

		if (written.empty())
		{
			continue;
		}

		std::vector<model::PolygonUv> list;

		for (auto const &one : written)
		{
			list.push_back(one.second);
		}

		std::string writeWhy;
		host::UvEditResult const result = host::UvEdit_WritePolygons(open, open->Level(), lead.id, list, &writeWhy);

		if (result != host::UvEditResult::Done)
		{
			left += list.size();
			firstWhy = firstWhy.empty() ? (writeWhy.empty() ? std::string(host::UvEditWords(result)) : writeWhy) : firstWhy;
			continue;
		}

		changed += list.size();
		ShowTheNewUv(open, lead, list);

		if (shapeFrames > 0)
		{
			host::AnimWrite write;

			write.shapeNumber = shapeNumber;
			write.anim = anim;
			animWrites.push_back(write);
			frames += shapeFrames;
		}
	}

	std::string animWhy;
	bool const animWritten = animWrites.empty() || host::AnimEdit_Write(open, animWrites, &animWhy) == host::AnimEditResult::Done;

	textureWords = "Texture Map: " + name + " done on " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s");

	if (allFrames)
	{
		textureWords += animWritten ? ", " + std::to_string(frames) + " animation frame" + (frames == 1 ? "" : "s") : ", the animation's frames NOT written (" + animWhy + ")";
	}

	if (notTextured)
	{
		textureWords += ", " + std::to_string(notTextured) + " not textured";
	}

	if (left)
	{
		textureWords += ", " + std::to_string(left) + " left as they were" + (firstWhy.empty() ? "" : " (" + firstWhy + ")");
	}

	statusWords = textureWords.c_str();
}
