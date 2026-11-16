// The measuring half of screen_picture.h: plain sums over a picture in memory, with no Windows or Direct3D call, so a test can try it
// on pictures it makes. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "screen_picture.h"

// A box cut to a picture's width by height: its first and one-past-last columns and rows, all within the picture
struct Span
{
	int32_t left;
	int32_t right;
	int32_t top;
	int32_t bottom;
};

static Span CutTo(const ui::Box &box, int32_t width, int32_t height)
{
	int64_t const left = box.x < 0 ? 0 : box.x;
	int64_t const top = box.y < 0 ? 0 : box.y;
	int64_t const right = (int64_t)box.x + (box.width > 0 ? box.width : 0);
	int64_t const bottom = (int64_t)box.y + (box.height > 0 ? box.height : 0);
	Span span;

	span.left = (int32_t)(left < width ? left : width);
	span.top = (int32_t)(top < height ? top : height);
	span.right = (int32_t)(right < span.left ? span.left : (right > width ? width : right));
	span.bottom = (int32_t)(bottom < span.top ? span.top : (bottom > height ? height : bottom));
	return span;
}

static int32_t Channel(uint32_t pixel, int shift)
{
	return (int32_t)((pixel >> shift) & 0xFF);
}

static bool CloseTo(int32_t a, int32_t b)
{
	return a - b <= kScreenPictureColourSlack && b - a <= kScreenPictureColourSlack;
}

void ScreenPicture_Measure(const uint32_t *pixels, int32_t width, int32_t height, int32_t rowPixels, const ui::Box &view, const ui::Box &patch,
	uint32_t panelsColour, ScreenPictureReport *report)
{
	report->panelsSeen = false;
	report->viewMean = 0.0;
	report->viewLitShare = 0.0;

	if (!pixels || width <= 0 || height <= 0 || rowPixels < width)
	{
		return;
	}

	Span const seen = CutTo(patch, width, height);
	bool allPanels = seen.right > seen.left && seen.bottom > seen.top;

	for (int32_t y = seen.top; y < seen.bottom && allPanels; y++)
	{
		for (int32_t x = seen.left; x < seen.right && allPanels; x++)
		{
			uint32_t const pixel = pixels[(size_t)y * (size_t)rowPixels + (size_t)x];

			allPanels = CloseTo(Channel(pixel, 16), Channel(panelsColour, 16)) && CloseTo(Channel(pixel, 8), Channel(panelsColour, 8))
				&& CloseTo(Channel(pixel, 0), Channel(panelsColour, 0));
		}
	}

	report->panelsSeen = allPanels;

	Span const inView = CutTo(view, width, height);
	double total = 0.0;
	double lit = 0.0;
	double count = 0.0;

	for (int32_t y = inView.top; y < inView.bottom; y++)
	{
		for (int32_t x = inView.left; x < inView.right; x++)
		{
			uint32_t const pixel = pixels[(size_t)y * (size_t)rowPixels + (size_t)x];
			double const brightness = (Channel(pixel, 16) + Channel(pixel, 8) + Channel(pixel, 0)) / 3.0;

			total += brightness;
			lit += brightness > 16.0 ? 1.0 : 0.0;
			count += 1.0;
		}
	}

	if (count > 0.0)
	{
		report->viewMean = total / count;
		report->viewLitShare = lit / count;
	}
}
