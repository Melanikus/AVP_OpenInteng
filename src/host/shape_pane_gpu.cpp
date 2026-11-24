// shape_pane_gpu.cpp: THE STAND-IN for the live panes' GPU side (shape_pane_gpu.h): every upload refused in words, nothing drawn, so the placement picker keeps
// its still pictures until Frost's GPU side replaces this file. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
#include "shape_pane_gpu.h"

namespace host
{
	int ShapePaneGpu_Upload(ShapePaneModel const &, std::string *why)
	{
		if (why)
		{
			*why = "the live panes' GPU side is not in yet";
		}

		return -1;
	}

	void ShapePaneGpu_Draw(int, int32_t const *, float)
	{
	}

	void ShapePaneGpu_FrameEnd()
	{
	}

	void ShapePaneGpu_Composite()
	{
	}

	void ShapePaneGpu_Forget(int)
	{
	}

	void ShapePaneGpu_ForgetAll()
	{
	}

	void ShapePaneGpu_FrameTime(uint32_t *lastUs, uint32_t *mostUs)
	{
		if (lastUs)
		{
			*lastUs = 0;
		}

		if (mostUs)
		{
			*mostUs = 0;
		}
	}
}
