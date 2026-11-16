// The test for src/host's device keeper: a lost device, windows that change size, a minimised window, the panels' swap chain, and resets
// that fail, against a pretend device. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// It needs no window and no graphics card. The pretend device keeps count of what it's asked, and notes any misuse a real one would refuse
// or be hurt by: anything let go of twice or made twice, or a reset while it still has its resources or its panels. Each check's words say
// what it proves. The program gives back a non-zero result if any check failed. tests/takeouts/device_keeper_test.json proves each check
// can fail.

#include <stdio.h>

#include <string>
#include <vector>

#include "device_keeper.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A pretend device. It says what it's told to, and after a reset that works it's fine, as a real one is.
class PretendDevice : public DeviceSeam
{
public:
	DeviceSays says = DeviceSays::Fine;
	bool resetWorks = true;
	bool remakeWorks = true;
	bool panelsWork = true;
	bool hasResources = true;   // a device is made with its resources, and no panels
	bool hasPanels = false;
	bool misused = false;

	int asks = 0;
	int letGos = 0;
	int resets = 0;
	int remakes = 0;
	int panelLetGos = 0;
	int panelMakes = 0;
	int32_t resetWidth = 0;
	int32_t resetHeight = 0;
	int32_t panelsWidth = 0;
	int32_t panelsHeight = 0;
	std::vector<uint32_t> resetTimes;
	uint32_t const *clock = nullptr;

	DeviceSays Ask() override
	{
		asks++;
		return says;
	}

	void LetGoOfResources() override
	{
		letGos++;
		misused = misused || !hasResources;
		hasResources = false;
	}

	bool Reset(int32_t width, int32_t height) override
	{
		resets++;
		resetWidth = width;
		resetHeight = height;
		resetTimes.push_back(clock ? *clock : 0);

		// A real device refuses a reset while anything is still in its default pool: its resources or a second swap chain
		if (hasResources || hasPanels)
		{
			misused = true;
			return false;
		}

		if (!resetWorks)
		{
			return false;
		}

		says = DeviceSays::Fine;
		return true;
	}

	bool RemakeResources() override
	{
		remakes++;
		misused = misused || hasResources;

		if (!remakeWorks)
		{
			return false;
		}

		hasResources = true;
		return true;
	}

	void LetGoOfPanels() override
	{
		panelLetGos++;
		misused = misused || !hasPanels;
		hasPanels = false;
	}

	bool MakePanels(int32_t width, int32_t height) override
	{
		panelMakes++;
		misused = misused || hasPanels;

		if (!panelsWork)
		{
			return false;
		}

		hasPanels = true;
		panelsWidth = width;
		panelsHeight = height;
		return true;
	}
};

// One frame where the view fills the parent: the same size for both
static bool Frame(DeviceKeeper &keeper, int32_t width, int32_t height, uint32_t nowMs)
{
	return keeper.BeforeFrame(width, height, width, height, nowMs);
}

static void TestDrawing()
{
	puts("A device that's fine:");

	PretendDevice device;
	DeviceKeeper keeper(device, 1024, 768);
	bool drawsEveryFrame = true;

	for (uint32_t t = 0; t < 100; t += 16)
	{
		drawsEveryFrame = drawsEveryFrame && Frame(keeper, 1024, 768, t);
	}

	Check(drawsEveryFrame && keeper.State() == SurfaceState::Drawing, "a fine device at the window's size draws every frame");
	Check(device.resets == 0 && device.letGos == 0 && device.remakes == 0 && device.asks == 7,
		"and is only asked, once a frame, never reset");
	Check(device.panelMakes == 1 && device.panelsWidth == 1024 && device.panelsHeight == 768 && keeper.HasPanels(),
		"its panels are made on the first frame, at the parent's size, and only then");
}

static void TestLost()
{
	puts("A lost device:");

	uint32_t now = 1000;
	PretendDevice device;
	device.clock = &now;
	DeviceKeeper keeper(device, 800, 600);
	Frame(keeper, 800, 600, now - 100);

	device.says = DeviceSays::Lost;
	Check(!Frame(keeper, 800, 600, now) && keeper.State() == SurfaceState::Waiting, "lost, the frame isn't drawn");
	Check(device.letGos == 1 && !device.hasResources && device.panelLetGos == 1 && !device.hasPanels,
		"and its resources and its panels are let go of");

	// Asked again too soon: not asked at all
	int const asksThen = device.asks;
	Check(!Frame(keeper, 800, 600, now + kLostAskAgainMs - 1) && device.asks == asksThen,
		"called again before 50 ms is up, it isn't even asked: no tight loop");
	Check(!Frame(keeper, 800, 600, now + kLostAskAgainMs) && device.asks == asksThen + 1 && device.letGos == 1 && device.panelLetGos == 1,
		"50 ms on, it's asked again, and still lost, it lets go of nothing more");

	// Ready to be reset
	now += 2 * kLostAskAgainMs;
	device.says = DeviceSays::ReadyToReset;
	Check(Frame(keeper, 800, 600, now) && keeper.State() == SurfaceState::Drawing, "ready to reset, it's reset and the frame draws");
	Check(device.resets == 1 && device.resetWidth == 800 && device.resetHeight == 600 && device.remakes == 1 && device.hasResources
		&& device.panelMakes == 2 && device.hasPanels, "at the window's size, with its resources and its panels made again after");
	Check(!device.misused, "never resetting while it had its resources or its panels, nor letting go of or making either twice");

	// Lost, then back by itself: only what was let go of is made again
	PretendDevice back;
	DeviceKeeper keeperBack(back, 800, 600);
	Frame(keeperBack, 800, 600, 0);
	back.says = DeviceSays::Lost;
	Frame(keeperBack, 800, 600, 10);
	back.says = DeviceSays::Fine;
	Check(Frame(keeperBack, 800, 600, 10 + kLostAskAgainMs) && back.resets == 0 && back.remakes == 1 && back.panelMakes == 2 && !back.misused,
		"lost and back by itself: no reset, only its resources and its panels made again");
}

static void TestResetsThatFail()
{
	puts("Resets that fail:");

	uint32_t now = 0;
	PretendDevice device;
	device.clock = &now;
	DeviceKeeper keeper(device, 640, 480);

	device.says = DeviceSays::ReadyToReset;
	device.resetWorks = false;

	// The waits between tries: 50, 100, 200, 400, 800, 1600, then 2000 from then on
	std::vector<uint32_t> tries;
	bool drewAny = false;

	// Called every millisecond for 30 seconds
	for (now = 0; now < 30000; now++)
	{
		size_t const before = device.resetTimes.size();
		drewAny = Frame(keeper, 640, 480, now) || drewAny;

		if (device.resetTimes.size() != before)
		{
			tries.push_back(now);
		}
	}

	Check(!drewAny, "with every reset failing, no frame is drawn");
	std::vector<uint32_t> gaps;

	for (size_t i = 1; i < tries.size(); i++)
	{
		gaps.push_back(tries[i] - tries[i - 1]);
	}

	bool const doubling = gaps.size() >= 7 && gaps[0] == 50 && gaps[1] == 100 && gaps[2] == 200 && gaps[3] == 400 && gaps[4] == 800
		&& gaps[5] == 1600 && gaps[6] == 2000;
	bool longestAfter = true;

	for (size_t i = 6; i < gaps.size(); i++)
	{
		longestAfter = longestAfter && gaps[i] == kResetLongestWaitMs;
	}

	Check(doubling, "a reset that fails is tried again after 50 ms, then 100, 200, 400, 800, 1600 and 2000");
	Check(longestAfter && keeper.State() == SurfaceState::Waiting, "and every 2 seconds from then on, never sooner");
	Check(device.resets == (int)tries.size() && keeper.ResetsFailed() == keeper.ResetsTried() && !device.misused,
		"each try one reset, each counted as failed, and nothing in its default pool misused");

	// It works at last: drawing again, and the wait starts from the first again
	device.resetWorks = true;
	now += kResetLongestWaitMs;
	Check(Frame(keeper, 640, 480, now) && keeper.NextWaitMs() == kResetFirstWaitMs, "when a reset works at last, it draws, and the wait starts again from 50 ms");
}

static void TestAThousandFailures()
{
	puts("A thousand resets that fail:");

	uint32_t now = 0;
	PretendDevice device;
	device.clock = &now;
	DeviceKeeper keeper(device, 1280, 720);
	device.says = DeviceSays::ReadyToReset;
	device.resetWorks = false;

	uint64_t calls = 0;

	while (keeper.ResetsFailed() < 1000 && calls < 10000000)
	{
		Frame(keeper, 1280, 720, now);
		now += 7;
		calls++;
	}

	bool neverTooSoon = true;

	for (size_t i = 1; i < device.resetTimes.size(); i++)
	{
		neverTooSoon = neverTooSoon && device.resetTimes[i] - device.resetTimes[i - 1] >= kResetFirstWaitMs;
	}

	Check(keeper.ResetsFailed() == 1000 && neverTooSoon, "a thousand failed resets, none sooner than 50 ms after the last");
	Check(calls > 100000 && device.resets == 1000, "though it was called " + std::to_string(calls) + " times, it reset only those 1,000");

	device.resetWorks = true;
	now += kResetLongestWaitMs;
	Check(Frame(keeper, 1280, 720, now) && keeper.State() == SurfaceState::Drawing && !device.misused, "and still comes back when a reset works");
}

static void TestSizes()
{
	puts("The windows' sizes:");

	PretendDevice device;
	DeviceKeeper keeper(device, 1024, 768);

	Check(Frame(keeper, 1280, 1024, 0) && device.resets == 1 && device.resetWidth == 1280 && device.resetHeight == 1024
		&& keeper.Width() == 1280 && keeper.Height() == 1024, "a new view size is a reset at that size, then the frame draws");
	Check(Frame(keeper, 1280, 1024, 16) && device.resets == 1, "the same size again needs no reset");

	// Minimised: nothing drawn, nothing asked, nothing reset
	int const asks = device.asks;
	Check(!keeper.BeforeFrame(0, 0, 0, 0, 32) && keeper.State() == SurfaceState::Minimised && device.asks == asks && device.resets == 1,
		"minimised, the parent 0 by 0: not drawn, and the device isn't even asked");
	Check(!keeper.BeforeFrame(1280, 1024, 0, 0, 33) && device.asks == asks, "however big the view says it is");
	Check(Frame(keeper, 1280, 1024, 48) && device.resets == 1, "back at its old size: drawing again, with no reset");
	Check(!keeper.BeforeFrame(0, 0, 0, 0, 64) && Frame(keeper, 900, 700, 80) && device.resets == 2 && device.resetWidth == 900,
		"back at a new size: reset at that size");

	struct SizeRow { int32_t width; int32_t height; int32_t wantWidth; int32_t wantHeight; char const *what; };
	SizeRow const rows[] =
	{
		{ 0, 500, 1, 500, "a side of 0 beside one that isn't is taken as 1" },
		{ -5, 600, 1, 600, "a side below 0 is taken as 0, so 1" },
		{ 100000, 100000, kSurfaceMostSide, kSurfaceMostSide, "a side above 16384 is taken as 16384" },
		{ INT32_MAX, 300, kSurfaceMostSide, 300, "even the largest there is" },
		{ INT32_MIN, 300, 1, 300, "and the smallest there is is taken as 1" },
	};

	for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
	{
		PretendDevice sized;
		DeviceKeeper sizedKeeper(sized, 640, 480);
		bool const drew = Frame(sizedKeeper, rows[i].width, rows[i].height, 0);
		Check(drew && sized.resetWidth == rows[i].wantWidth && sized.resetHeight == rows[i].wantHeight && sized.panelsWidth == rows[i].wantWidth
			&& sized.panelsHeight == rows[i].wantHeight, std::string(rows[i].what) + ", for the view and the parent alike");
	}

	PretendDevice belowBoth;
	DeviceKeeper belowKeeper(belowBoth, 640, 480);
	Check(!Frame(belowKeeper, -1, -1, 0) && belowKeeper.State() == SurfaceState::Minimised && belowBoth.asks == 0,
		"a parent with both sides below 0 is 0 by 0: minimised");

	PretendDevice made;
	DeviceKeeper madeKeeper(made, -10, 0);
	Check(madeKeeper.Width() == 1 && madeKeeper.Height() == 1, "a device said to be made at a size below 1 is taken as 1 by 1");
}

static void TestPanels()
{
	puts("The panels' swap chain:");

	PretendDevice device;
	DeviceKeeper keeper(device, 800, 600);
	keeper.BeforeFrame(800, 600, 1200, 900, 0);
	Check(device.panelsWidth == 1200 && device.panelsHeight == 900 && keeper.PanelsWidth() == 1200 && keeper.PanelsHeight() == 900,
		"the panels are made at the parent's size, not the view's");

	// Only the parent's size changes: only the panels are made again
	Check(keeper.BeforeFrame(800, 600, 1300, 950, 16) && device.resets == 0 && device.panelLetGos == 1 && device.panelMakes == 2
		&& device.panelsWidth == 1300 && device.panelsHeight == 950, "a new size for the parent alone makes only the panels again: no reset");

	// The view's size changes: a reset, with the panels let go of first and made after
	Check(keeper.BeforeFrame(700, 500, 1300, 950, 32) && device.resets == 1 && device.resetWidth == 700 && device.panelMakes == 3
		&& device.panelsWidth == 1300 && !device.misused, "a new view size is a reset, with the panels let go of before it and made after");

	// The layout gives the view no room: drawn 1 by 1, so the panels still are
	Check(keeper.BeforeFrame(0, 0, 400, 300, 48) && device.resetWidth == 1 && device.resetHeight == 1 && device.panelsWidth == 400,
		"a view with no room, in a parent that isn't minimised, is drawn 1 by 1, and the panels still are");

	// The panels can't be made: not drawn, and tried again later
	uint32_t now = 1000;
	PretendDevice noPanels;
	noPanels.clock = &now;
	noPanels.panelsWork = false;
	DeviceKeeper noPanelsKeeper(noPanels, 640, 480);
	Check(!Frame(noPanelsKeeper, 640, 480, now) && noPanelsKeeper.State() == SurfaceState::Waiting && !noPanelsKeeper.HasPanels(),
		"panels that can't be made: not drawn, and waiting");
	int const makesThen = noPanels.panelMakes;
	Check(!Frame(noPanelsKeeper, 640, 480, now + kResetFirstWaitMs - 1) && noPanels.panelMakes == makesThen,
		"and not tried again before the wait is up");
	noPanels.panelsWork = true;
	Check(Frame(noPanelsKeeper, 640, 480, now + kResetFirstWaitMs) && noPanels.panelMakes == makesThen + 1 && noPanels.resets == 0 && !noPanels.misused,
		"then made, with no reset");

	// The panels' window given, or changed: the panels are made again on the next frame
	PretendDevice changed;
	DeviceKeeper changedKeeper(changed, 640, 480);
	Frame(changedKeeper, 640, 480, 0);
	changedKeeper.PanelsWindowChanged();
	Check(!changedKeeper.HasPanels() && changed.panelLetGos == 1 && Frame(changedKeeper, 640, 480, 16) && changed.panelMakes == 2 && !changed.misused,
		"a new window for the panels lets the old ones go, and the next frame makes them again");
	PretendDevice none;
	DeviceKeeper noneKeeper(none, 640, 480);
	noneKeeper.PanelsWindowChanged();
	Check(none.panelLetGos == 0 && !none.misused, "and with none made yet, lets go of nothing");
}

static void TestLostDuringAResize()
{
	puts("Lost while the windows change size:");

	uint32_t now = 0;
	PretendDevice device;
	device.clock = &now;
	DeviceKeeper keeper(device, 1024, 768);
	Frame(keeper, 1024, 768, now);

	device.says = DeviceSays::Lost;
	now += 1;
	Frame(keeper, 1024, 768, now);
	now += kLostAskAgainMs;
	keeper.BeforeFrame(1100, 800, 1500, 1000, now);
	now += kLostAskAgainMs;
	keeper.BeforeFrame(1200, 900, 1600, 1100, now);
	Check(device.resets == 0 && device.panelMakes == 1, "while lost, a new size resets nothing and makes no panels");

	// Minimised while lost, then back at yet other sizes, and ready to reset
	keeper.BeforeFrame(0, 0, 0, 0, now + 1);
	now += kLostAskAgainMs;
	device.says = DeviceSays::ReadyToReset;
	Check(keeper.BeforeFrame(1300, 950, 1700, 1200, now) && device.resets == 1 && device.resetWidth == 1300 && device.resetHeight == 950
		&& device.panelsWidth == 1700 && device.panelsHeight == 1200, "ready at last, it's reset once, at the sizes the windows have now");
	Check(!device.misused && device.letGos == 1 && device.remakes == 1 && device.panelLetGos == 1 && device.panelMakes == 2,
		"with its resources and its panels let go of once and made once");
}

static void TestBroken()
{
	puts("A broken device, and resources that won't come back:");

	PretendDevice device;
	DeviceKeeper keeper(device, 640, 480);
	device.says = DeviceSays::Broken;
	Check(!Frame(keeper, 640, 480, 0) && keeper.State() == SurfaceState::Broken, "a device that says it can't go on is broken");

	device.says = DeviceSays::Fine;
	int const asks = device.asks;
	Check(!Frame(keeper, 640, 480, 10000) && device.asks == asks, "and never drawn or asked again");

	// Reset, but its resources can't be made: tried again later, with no second reset
	uint32_t now = 0;
	PretendDevice noRemake;
	noRemake.clock = &now;
	DeviceKeeper remakeKeeper(noRemake, 640, 480);
	noRemake.says = DeviceSays::ReadyToReset;
	noRemake.remakeWorks = false;
	Check(!Frame(remakeKeeper, 640, 480, now) && noRemake.resets == 1 && remakeKeeper.State() == SurfaceState::Waiting,
		"reset, but its resources can't be made: not drawn, and waiting");
	noRemake.remakeWorks = true;
	now += kResetFirstWaitMs;
	Check(Frame(remakeKeeper, 640, 480, now) && noRemake.resets == 1 && noRemake.remakes == 2 && !noRemake.misused,
		"the next try makes them, with no second reset");
}

static void TestClockWrapping()
{
	puts("A clock that wraps round:");

	uint32_t now = 0xFFFFFFF0u;
	PretendDevice device;
	device.clock = &now;
	DeviceKeeper keeper(device, 640, 480);
	device.says = DeviceSays::Lost;
	Frame(keeper, 640, 480, now);
	int const asks = device.asks;
	Check(!Frame(keeper, 640, 480, now + 10) && device.asks == asks, "10 ms on, past the clock's wrap, it's still too soon to ask");
	Check(!Frame(keeper, 640, 480, now + kLostAskAgainMs) && device.asks == asks + 1, "and 50 ms on, past the wrap, it's asked");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestDrawing();
	TestLost();
	TestResetsThatFail();
	TestAThousandFailures();
	TestSizes();
	TestPanels();
	TestLostDuringAResize();
	TestBroken();
	TestClockWrapping();

	printf("%s: %d check(s) failed\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed", checksFailed);
	return checksFailed ? 1 : 0;
}
