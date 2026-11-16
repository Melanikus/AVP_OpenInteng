// See click_mode.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P4). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include "click_mode.h"

namespace host
{
	namespace
	{
		bool waiting = false;
		int32_t waitingCommand = 0;
		ClickWants waitingFor = ClickWants::Nothing;
		char commandName[kClickModeMostCommandWords] = { 0 };
		char words[kClickModeMostWords] = { 0 };

		const char *Kind(ClickWants wants)
		{
			switch (wants)
			{
				case ClickWants::Object:
					return "an object";
				case ClickWants::Module:
					return "a module";
				case ClickWants::Polygon:
					return "a polygon";
				case ClickWants::Corner:
					return "a corner";
				default:
					return "something";
			}
		}

		bool KnownKind(ClickWants wants)
		{
			return wants == ClickWants::Object || wants == ClickWants::Module || wants == ClickWants::Polygon || wants == ClickWants::Corner;
		}

		// What the click found, in a few words, for "that is not what was waited for"
		const char *Found(const PickReport &report)
		{
			switch (report.found)
			{
				case PickFound::Object:
					return "an object";
				case PickFound::Module:
					return "a module";
				case PickFound::NotInTheFile:
					return "something the game made, not the level";
				case PickFound::NumberRefused:
					return "a module the game doesn't list";
				case PickFound::Nothing:
					return "nothing";
				default:
					return "nothing the view could pick";
			}
		}

		void SayWaiting()
		{
			_snprintf_s(words, sizeof(words), _TRUNCATE, "%s - click %s in the view (Esc cancels)", commandName, Kind(waitingFor));
		}
	}

	bool ClickMode_Start(int32_t command, ClickWants wants, const char *commandWords)
	{
		if (!KnownKind(wants) || !commandWords || commandWords[0] == '\0')
		{
			return false;
		}

		size_t i = 0;

		for (; i < kClickModeMostCommandWords - 1 && commandWords[i] != '\0'; i++)
		{
			unsigned char const letter = (unsigned char)commandWords[i];

			commandName[i] = (letter >= ' ' && letter <= '~') ? (char)letter : '?';
		}

		commandName[i] = '\0';
		waiting = true;
		waitingCommand = command;
		waitingFor = wants;
		SayWaiting();
		return true;
	}

	bool ClickMode_Cancel()
	{
		bool const was = waiting;

		waiting = false;
		waitingFor = ClickWants::Nothing;
		words[0] = '\0';
		return was;
	}

	bool ClickMode_Waiting()
	{
		return waiting;
	}

	ClickWants ClickMode_Wants()
	{
		return waiting ? waitingFor : ClickWants::Nothing;
	}

	const char *ClickMode_Words()
	{
		return words;
	}

	bool ClickMode_Satisfies(ClickWants wants, const PickReport &report)
	{
		bool const hitSomethingTheLevelNames = report.found == PickFound::Object || report.found == PickFound::Module;

		switch (wants)
		{
			case ClickWants::Object:
				return report.found == PickFound::Object;
			case ClickWants::Module:
				return report.found == PickFound::Module;
			case ClickWants::Polygon:
				return hitSomethingTheLevelNames && report.hasHit && report.polygonNumber >= 0;
			case ClickWants::Corner:
				return hitSomethingTheLevelNames && report.hasHit && report.polygonNumber >= 0 && report.hasCorner;
			default:
				return false;
		}
	}

	ClickAnswer ClickMode_Click(const PickReport &report)
	{
		ClickAnswer answer;

		memset(&answer, 0, sizeof(answer));

		if (!waiting)
		{
			return answer;
		}

		if (!ClickMode_Satisfies(waitingFor, report))
		{
			_snprintf_s(words, sizeof(words), _TRUNCATE, "%s: that is %s, not %s - click %s (Esc cancels)", commandName, Found(report), Kind(waitingFor),
				Kind(waitingFor));
			return answer;
		}

		answer.done = true;
		answer.command = waitingCommand;
		answer.wants = waitingFor;
		answer.report = report;
		ClickMode_Cancel();
		return answer;
	}
}
