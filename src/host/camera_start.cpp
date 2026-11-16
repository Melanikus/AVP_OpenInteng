// See camera_start.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "camera_start.h"

CameraPlace CameraStart_Choose(bool hasPlayer, bool hasStrategy, bool hasDynamics, const CameraPlace &body, const CameraPlace &levelStart,
	const char **why)
{
	if (!hasPlayer)
	{
		*why = "the level made no player, so the camera starts at the level's own start";
		return levelStart;
	}

	if (!hasStrategy)
	{
		*why = "the player has no strategy block, so the camera starts at the level's own start";
		return levelStart;
	}

	if (!hasDynamics)
	{
		*why = "the player has no body (no dynamics block), so the camera starts at the level's own start";
		return levelStart;
	}

	*why = 0;
	return body;
}
