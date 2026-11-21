// autoplot_waits.cpp: AN EDIT WAITS FOR THE PLOT (autoplot_waits.h).
// Written 2026-11-21 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autoplot_waits.h"

namespace host
{
	bool AutoPlot_ItemWaits(IntengWork work)
	{
		// the kinds that only LOOK (each one's comment in inteng_menus.h says what it does): they change how the level is drawn or what is said, never the level.
		// Every other kind waits, a kind added later included, until it is put here
		switch (work)
		{
		case IntengWork::Keys:                   // the keys' lists and help
		case IntengWork::ConstructionKeys:
		case IntengWork::PlacementKeys:
		case IntengWork::LightHelp:
		case IntengWork::LinkHelp:
		case IntengWork::Screenshot:             // the view saved as a picture
		case IntengWork::Find:                   // the view flies to an object
		case IntengWork::ViewPolygon:            // the draw modes
		case IntengWork::ViewPolyline:
		case IntengWork::ViewFilledPolyline:
		case IntengWork::ViewLighting:
		case IntengWork::MixedView:              // the module screens' ways of drawing
		case IntengWork::ShowAllModules:
		case IntengWork::ViewDistanceToggle:
		case IntengWork::OnlyLinkedToggle:
		case IntengWork::LinkView:
		case IntengWork::AdjacentViewDistance:
		case IntengWork::AdjacentShowAll:
		case IntengWork::ShowPolyCounts:
		case IntengWork::PlacementPolyCounts:
		case IntengWork::SplitScreenToggle:      // three more views beside the main one
		case IntengWork::SelectByPolygons:       // the options kept in the settings file (editor_options.h)
		case IntengWork::PreferBmp:
		case IntengWork::PolygonViewDistance:
		case IntengWork::TextScale:              // the text size
		case IntengWork::Quit:                   // the editor closes: the plot is stopped and waited for first (AutoPlotCommand_Stop)
			return false;

		default:
			return true;
		}
	}

	std::string AutoPlot_WaitWords(char const *itemName)
	{
		std::string const name = itemName && itemName[0] ? itemName : "That item";

		return name + " waits for the auto-plot: it is writing the level (choose it again when the plot has ended)";
	}
}
