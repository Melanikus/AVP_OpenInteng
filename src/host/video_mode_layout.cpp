// video_mode_layout.cpp: where the Video screen puts things (video_mode_layout.h says what and why).
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "video_mode_layout.h"

#include <math.h>

namespace host
{
	namespace
	{
		// A number of pixels held to what an int32_t box can take (a hostile size never wraps round)
		int32_t Held(double value)
		{
			if (!(value > -1000000.0))
			{
				return -1000000;
			}

			return value > 1000000.0 ? 1000000 : (int32_t)floor(value + 0.5);
		}
	}

	VideoModeLayout VideoModeLayout_Plan(VideoLayoutBox const &view, std::vector<int32_t> const &labelWidths, int32_t fontHeight, size_t lines, uint32_t sourceWidth,
		uint32_t sourceHeight, double const *sourceRect)
	{
		VideoModeLayout plan;

		plan.controls.resize(labelWidths.size());

		if (view.width <= 2 * kVideoLayoutMargin || view.height <= 2 * kVideoLayoutMargin || fontHeight <= 0 || fontHeight > 1000)
		{
			return plan;   // a view too small for anything, or a font of no size: nothing is placed
		}

		// THE LINES OF WORDS at the top, one under the other
		int32_t top = view.y + kVideoLayoutMargin;

		for (size_t i = 0; i < lines && i < 64; i++)
		{
			plan.lineTops.push_back(top);
			top += fontHeight + kVideoLayoutLineGap;
		}

		int32_t const pictureTop = lines > 0 ? top + kVideoLayoutMargin - kVideoLayoutLineGap : view.y + kVideoLayoutMargin;

		// THE CONTROLS: in rows as wide as the view allows, each row centred; first measured, then placed from the view's foot up
		int32_t const buttonHeight = fontHeight + 2 * kVideoLayoutButtonPadY;
		int32_t const rowRoom = view.width - 2 * kVideoLayoutMargin - 2 * kVideoLayoutFramePad;
		std::vector<std::vector<size_t>> rows;
		std::vector<int32_t> rowWidths;

		for (size_t i = 0; i < labelWidths.size(); i++)
		{
			if (labelWidths[i] <= 0 || rowRoom <= 2 * kVideoLayoutButtonPadX)
			{
				continue;   // no words, or no room in the view for any button
			}

			// its words and their room, never wider than a row: a longer label is cut by the view, its button stays inside it
			int32_t const wanted = (labelWidths[i] > 4000 ? 4000 : labelWidths[i]) + 2 * kVideoLayoutButtonPadX;
			int32_t const width = wanted > rowRoom ? rowRoom : wanted;

			if (rows.empty() || rowWidths.back() + kVideoLayoutButtonGap + width > rowRoom)
			{
				rows.push_back(std::vector<size_t>());
				rowWidths.push_back(-kVideoLayoutButtonGap);
			}

			rows.back().push_back(i);
			rowWidths.back() += kVideoLayoutButtonGap + width;
			plan.controls[i].width = width;
			plan.controls[i].height = buttonHeight;
		}

		int32_t pictureBottom = view.y + view.height - kVideoLayoutMargin;

		if (!rows.empty())
		{
			int32_t widest = 0;

			for (int32_t const width : rowWidths)
			{
				widest = width > widest ? width : widest;
			}

			int32_t const framedWidth = widest + 2 * kVideoLayoutFramePad;
			int32_t const framedHeight = (int32_t)rows.size() * buttonHeight + ((int32_t)rows.size() - 1) * kVideoLayoutButtonGap + 2 * kVideoLayoutFramePad;

			plan.frame.width = framedWidth;
			plan.frame.height = framedHeight;
			plan.frame.x = view.x + (view.width - framedWidth) / 2;
			plan.frame.y = view.y + view.height - kVideoLayoutMargin - framedHeight;
			plan.frame.y = plan.frame.y < view.y ? view.y : plan.frame.y;

			for (size_t r = 0; r < rows.size(); r++)
			{
				int32_t x = view.x + (view.width - rowWidths[r]) / 2;
				int32_t const y = plan.frame.y + kVideoLayoutFramePad + (int32_t)r * (buttonHeight + kVideoLayoutButtonGap);

				for (size_t const i : rows[r])
				{
					plan.controls[i].x = x;
					plan.controls[i].y = y;
					x += plan.controls[i].width + kVideoLayoutButtonGap;
				}
			}

			pictureBottom = plan.frame.y - kVideoLayoutMargin;
		}

		// THE PICTURE AND ITS RECTANGLE, fitted as one into the room between the words and the controls
		int32_t const roomX = view.x + kVideoLayoutMargin;
		int32_t const roomWidth = view.width - 2 * kVideoLayoutMargin;
		int32_t const roomHeight = pictureBottom - pictureTop;

		if (sourceWidth == 0 || sourceHeight == 0 || sourceWidth > 100000 || sourceHeight > 100000 || roomWidth < 8 || roomHeight < 8)
		{
			return plan;
		}

		double rect[4] = { 0.0, 0.0, (double)sourceWidth, (double)sourceHeight };
		bool const rectGood = sourceRect != nullptr && isfinite(sourceRect[0]) && isfinite(sourceRect[1]) && isfinite(sourceRect[2]) && isfinite(sourceRect[3]) &&
			sourceRect[2] > 0.0 && sourceRect[3] > 0.0 && fabs(sourceRect[0]) < 1.0e6 && fabs(sourceRect[1]) < 1.0e6 && sourceRect[2] < 1.0e6 && sourceRect[3] < 1.0e6;

		if (rectGood)
		{
			for (int k = 0; k < 4; k++)
			{
				rect[k] = sourceRect[k];
			}
		}

		// the two together, in the picture's pixels
		double const left = rect[0] < 0.0 ? rect[0] : 0.0;
		double const topSource = rect[1] < 0.0 ? rect[1] : 0.0;
		double const right = rect[0] + rect[2] > (double)sourceWidth ? rect[0] + rect[2] : (double)sourceWidth;
		double const bottom = rect[1] + rect[3] > (double)sourceHeight ? rect[1] + rect[3] : (double)sourceHeight;
		double const scaleX = (double)roomWidth / (right - left);
		double const scaleY = (double)roomHeight / (bottom - topSource);
		double const scale = scaleX < scaleY ? scaleX : scaleY;
		double const originX = roomX + ((double)roomWidth - (right - left) * scale) / 2.0 - left * scale;
		double const originY = pictureTop + ((double)roomHeight - (bottom - topSource) * scale) / 2.0 - topSource * scale;

		plan.picture.x = Held(originX);
		plan.picture.y = Held(originY);
		plan.picture.width = Held((double)sourceWidth * scale);
		plan.picture.height = Held((double)sourceHeight * scale);

		if (plan.picture.width < 1 || plan.picture.height < 1)
		{
			plan.picture = VideoLayoutBox();
			return plan;
		}

		if (rectGood)
		{
			plan.rectangle.x = Held(originX + rect[0] * scale);
			plan.rectangle.y = Held(originY + rect[1] * scale);
			plan.rectangle.width = Held(rect[2] * scale);
			plan.rectangle.height = Held(rect[3] * scale);
		}

		return plan;
	}

	int32_t VideoModeLayout_At(std::vector<VideoLayoutBox> const &boxes, int32_t x, int32_t y)
	{
		for (size_t i = 0; i < boxes.size(); i++)
		{
			VideoLayoutBox const &box = boxes[i];

			if (box.width > 0 && box.height > 0 && x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height)
			{
				return (int32_t)i;
			}
		}

		return -1;
	}
}
