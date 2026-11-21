// host_can_crawl_offer_test.cpp: the Can Crawl offer's window (can_crawl_offer.h; the owner 11-21): its title, the owner's words in lines that give them back whole, and the two
// buttons with what each answers. Written 2026-11-21 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>
#include <vector>

#include "can_crawl_offer.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

int main()
{
	printf("host_can_crawl_offer_test\n");

	std::vector<std::string> const lines = host::CanCrawlOffer_Lines(host::kCanCrawlOfferText, host::kCanCrawlOfferLineLetters);
	std::string joined;
	bool short_ = !lines.empty();

	for (size_t i = 0; i < lines.size(); i++)
	{
		joined += (i ? " " : "") + lines[i];
		short_ = short_ && lines[i].size() <= host::kCanCrawlOfferLineLetters;
	}

	Check(joined == host::kCanCrawlOfferText, "the owner's words, broken into lines, give them back whole (" + std::to_string((unsigned long long)lines.size()) + " lines)");
	Check(short_, "each line at most 52 letters");
	Check(host::CanCrawlOffer_Lines("one two", 3) == std::vector<std::string>({ "one", "two" }) && host::CanCrawlOffer_Lines("abcdefgh x", 3) ==
		std::vector<std::string>({ "abcdefgh", "x" }), "broken at spaces; a word longer than a line is a line of its own");

	host::DialogFormSpec const form = host::CanCrawlOffer_Form();
	std::string labels, buttons;
	int32_t apply = -1, no = -1;

	for (host::DialogWidget const &w : form.widgets)
	{
		if (w.type == host::DialogWidgetType::Label)
		{
			labels += (labels.empty() ? "" : " ") + w.text;
		}
		else if (w.type == host::DialogWidgetType::Button)
		{
			buttons += (buttons.empty() ? "" : "|") + w.text;
			apply = w.text == host::kCanCrawlOfferApply ? w.answer : apply;
			no = w.text == host::kCanCrawlOfferNo ? w.answer : no;
		}
	}

	Check(form.title == "Can Crawl", "the window's title: Can Crawl (" + form.title + ")");
	Check(labels == host::kCanCrawlOfferText, "its words are the owner's, whole");
	Check(buttons == "Apply Module Flag|No", "two buttons, Apply Module Flag and No (" + buttons + ")");
	Check(apply == host::kDialogOk && no == host::kDialogNo, "Apply Module Flag answers Ok, No answers No");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
