// can_crawl_offer.h: THE CAN CRAWL OFFER (the owner 11-21, alpha.3, through Bishop: "let's enable it, when you do it it pops up a message ..."). Waypoint mode's Add Can Crawl
// (0x16) and the W key (0x27) set the waypoint's own flags bit 0 (wayflag_cancrawl) exactly as inteng does, so the file stays as inteng would write it; but the game reads that bit
// only inside an "#if 0" in AlienIsEncouragedToCrawl (AvP 3dc/avp/bh_waypoint.cpp 870-883): it does nothing. So, when the bit is turned ON, the editor offers the module's
// Airduct flag instead (MODULEFLAG_AIRDUCT 0x80000000: aliens crawl there, bh_near.cpp 2013-2018; marines, bh_marine.cpp 10419-10423; predators, bh_predator.cpp 4298-4302).
// This file: the offer's words, in one place, and its window (Apply Module Flag / No). Pure. Written 2026-11-21 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>

#include <string>
#include <vector>

#include "dialog_box.h"

namespace host
{
	// The owner's words (11-21 04:4x, through Bishop), with "all NPCs ... on this module" made "aliens, marines and predators ... in this module": the flag is read by those three
	// only (the queen, praetorians, xenoborgs and facehuggers never look at it); the owner, 04:5x: "Use that"
	char const *const kCanCrawlOfferText = "This functionality never worked in inteng, alternatively the \"airduct\" flag can be applied, which will cause aliens, marines and predators to "
		"crawl in this module, would you like to do that instead?";
	char const *const kCanCrawlOfferTitle = "Can Crawl";
	char const *const kCanCrawlOfferApply = "Apply Module Flag";   // answers kDialogOk: the Airduct flag goes on the module
	char const *const kCanCrawlOfferNo = "No";                     // answers kDialogNo: the module is left as it is
	size_t const kCanCrawlOfferLineLetters = 52;                   // the words broken into lines of at most this many letters (at a space), one label each

	// The words in lines of at most `most` letters, broken at spaces (a word longer than a line is a line of its own); joined with a space they are the words again
	inline std::vector<std::string> CanCrawlOffer_Lines(std::string const &words, size_t most)
	{
		std::vector<std::string> lines;
		std::string line;
		size_t at = 0;

		while (at < words.size())
		{
			size_t const space = words.find(' ', at);
			std::string const word = words.substr(at, space == std::string::npos ? std::string::npos : space - at);

			if (!line.empty() && line.size() + 1 + word.size() > most)
			{
				lines.push_back(line);
				line.clear();
			}

			line += (line.empty() ? "" : " ") + word;
			at = space == std::string::npos ? words.size() : space + 1;
		}

		if (!line.empty())
		{
			lines.push_back(line);
		}

		return lines;
	}

	// The offer's window: the title, the words in lines (a label each, 0x12 apart, as the text dialog's rows), and the two buttons under them (the plot dialog's look:
	// alien_auto_waypoint.cpp's Ok and Cancel)
	inline DialogFormSpec CanCrawlOffer_Form()
	{
		DialogFormSpec spec;
		std::vector<std::string> const lines = CanCrawlOffer_Lines(kCanCrawlOfferText, kCanCrawlOfferLineLetters);
		int32_t y = 0x1e;

		spec.title = kCanCrawlOfferTitle;
		spec.width = 400;

		for (std::string const &line : lines)
		{
			DialogWidget label;

			label.type = DialogWidgetType::Label;
			label.box = DialogRect{ 0x14, y, 360, 0x12 };
			label.text = line;
			spec.widgets.push_back(label);
			y += 0x12;
		}

		DialogWidget apply, no;

		apply.type = DialogWidgetType::Button;
		apply.box = DialogRect{ 0x50, y + 0x10, 0x82, 0x10 };
		apply.text = kCanCrawlOfferApply;
		apply.answer = kDialogOk;
		no.type = DialogWidgetType::Button;
		no.box = DialogRect{ 0xf0, y + 0x10, 0x32, 0x10 };
		no.text = kCanCrawlOfferNo;
		no.answer = kDialogNo;
		spec.widgets.push_back(apply);
		spec.widgets.push_back(no);
		spec.height = y + 0x10 + 0x10 + 0x1e;
		return spec;
	}
}
