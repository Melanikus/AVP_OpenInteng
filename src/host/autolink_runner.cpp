// autolink_runner.cpp: AutoLink's run on a worker thread, the frame the one writer of the trees (autolink_runner.h says what inteng does and what is named).
// Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "autolink_runner.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <exception>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <utility>

#include "level_chunks.h"
#include "level_file.h"
#include "autolink_records.h"
#include "autolink_render.h"
#include "autolink_run.h"
#include "module_links.h"

namespace host
{
	struct AutoLinkRunner::Parts
	{
		// ---- set by Start, then only read (the worker reads the records; the frame reads objectIndexOf and the module numbers)
		AutoLinkRecords records;
		std::vector<int32_t> order;
		std::vector<AutoLinkView> views;   // Load from file: one view per entry of order (empty: AutoLink's own views)
		bool deleteOld = false;
		int32_t distance = 0;
		OpenLevelFile *open = nullptr;

		// ---- the frame's own
		std::map<int32_t, std::unique_ptr<LinkTree> > trees;   // by object number
		LinkTree *tree = nullptr;                               // the tree of the module in hand (inteng's DAT_00704374)
		AutoLinkTreeWriter writer;
		AutoLinkRunCounts counts;
		bool started = false;
		bool finished = false;
		bool ended = false;
		bool stoppedWell = true;
		std::string stopWhy;

		// ---- between the threads
		struct Message
		{
			enum class Kind
			{
				Begun,      // the module's work begins (the frame makes it current: FUN_004c10c0)
				Events,     // its pictures' events, in order
				Finished,   // the worker is done (ok false: it stopped on a fault, why says it)
			};

			Kind kind;
			int32_t record;
			std::vector<AutoLinkEvent> events;
			size_t pictures;
			bool ok;
			std::string why;
		};

		std::mutex lock;
		std::deque<Message> queue;
		std::deque<std::vector<uint32_t> > shownPictures;
		std::atomic<bool> cancel{ false };
		std::thread worker;

		void Post(Message &&message)
		{
			std::lock_guard<std::mutex> hold(lock);

			queue.push_back(std::move(message));
		}

		void Finish(bool ok, std::string const &why)
		{
			Post(Message{ Message::Kind::Finished, -1, {}, 0, ok, why });
		}

		// The worker: FUN_004c8930's loop, the pictures drawn by a renderer of its own
		void Work()
		{
			try
			{
				AutoLinkRenderer renderer;
				std::string why;
				size_t leftOut = 0;

				if (!renderer.Open(&why) || !renderer.Load(records.records, &leftOut, &why))
				{
					Finish(false, "AutoLink's pictures can't be drawn: " + why);
					return;
				}

				std::vector<uint8_t> shown(records.records.size(), 1);   // FUN_004c6380 0x4c6c41-0x4c6c66: every record drawn

				AutoLinkPictureMaker const picture = [this, &renderer](AutoLinkView const &view, std::vector<uint8_t> const &drawn, std::vector<uint32_t> *pixels, std::string *pictureWhy)
				{
					if (!renderer.Picture(view, drawn, pixels, pictureWhy))
					{
						return false;
					}

					std::lock_guard<std::mutex> hold(lock);

					shownPictures.push_back(*pixels);

					while (shownPictures.size() > kAutoLinkShowKept)
					{
						shownPictures.pop_front();
					}

					return true;
				};

				for (size_t n = 0; n < order.size(); n++)
				{
					int32_t const a = order[n];

					// 0x4c899c (Load from file: 0x4c8cd5): a right press, read at the top of each record, ends the loop
					if (cancel.load())
					{
						renderer.Close();
						Finish(true, "cancelled by a right press");
						return;
					}

					Post(Message{ Message::Kind::Begun, a, {}, 0, true, std::string() });

					std::vector<AutoLinkEvent> events;
					size_t pictures = 0;
					bool const ok = views.empty() ? AutoLink_ModuleWork(records.records, records.standsFor, a, distance, &shown, picture, &events, &pictures, &why) :
						AutoLink_ViewWork(records.records, records.standsFor, a, views[n], distance, &shown, picture, &events, &pictures, &why);

					Post(Message{ Message::Kind::Events, a, std::move(events), pictures, ok, std::string() });

					if (!ok)
					{
						renderer.Close();
						Finish(false, why);
						return;
					}
				}

				renderer.Close();
				Finish(true, std::string());
			}
			catch (std::exception const &)
			{
				Finish(false, "out of memory");
			}
		}
	};

	AutoLinkRunner::AutoLinkRunner()
		: parts(new Parts)
	{
	}

	AutoLinkRunner::~AutoLinkRunner()
	{
		if (parts && parts->worker.joinable())
		{
			parts->cancel.store(true);
			parts->worker.join();
		}
	}

	bool AutoLinkRunner::Start(OpenLevelFile *open, std::vector<int32_t> const *picks, bool deleteOld, double levelScale, int32_t visibleDistance, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		if (Running())
		{
			*why = "a run is on already";
			return false;
		}

		if (!open || !open->Level())
		{
			*why = "no level is open";
			return false;
		}

		if (parts->worker.joinable())
		{
			parts->worker.join();
		}

		parts.reset(new Parts);

		Parts &p = *parts;

		AutoLink_Records(open, levelScale > 0.0 ? levelScale : 1.0, &p.records);

		if (p.records.objectIndexOf.size() != p.records.records.size() || p.records.standsFor.size() != p.records.records.size())
		{
			*why = "the level's records could not be read";
			return false;
		}

		// 0x4c8997-0x4c89f0: each record in number order with a file object (a module: no stand-in), and, given a list, in it
		for (size_t n = 0; n < p.records.records.size(); n++)
		{
			if (p.records.standsFor[n] > -1)
			{
				continue;
			}

			if (picks && std::find(picks->begin(), picks->end(), p.records.objectIndexOf[n]) == picks->end())
			{
				continue;
			}

			p.order.push_back((int32_t)n);
		}

		if (p.order.empty())
		{
			*why = picks ? "none of the picked modules has a shape" : "the level has no module with a shape";
			return false;
		}

		p.deleteOld = deleteOld;
		p.distance = visibleDistance;
		p.open = open;
		p.started = true;
		p.worker = std::thread([&p]() { p.Work(); });
		return true;
	}

	bool AutoLinkRunner::StartFromFile(OpenLevelFile *open, std::vector<FileView> const &views, double levelScale, int32_t visibleDistance, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		if (Running())
		{
			*why = "a run is on already";
			return false;
		}

		if (!open || !open->Level())
		{
			*why = "no level is open";
			return false;
		}

		if (parts->worker.joinable())
		{
			parts->worker.join();
		}

		parts.reset(new Parts);

		Parts &p = *parts;

		AutoLink_Records(open, levelScale > 0.0 ? levelScale : 1.0, &p.records);

		if (p.records.objectIndexOf.size() != p.records.records.size() || p.records.standsFor.size() != p.records.records.size())
		{
			*why = "the level's records could not be read";
			return false;
		}

		// each entry's module: its record (a module's, not a stand-in's)
		for (FileView const &one : views)
		{
			for (size_t n = 0; n < p.records.records.size(); n++)
			{
				if (p.records.standsFor[n] < 0 && p.records.objectIndexOf[n] == one.object)
				{
					p.order.push_back((int32_t)n);
					p.views.push_back(one.view);
					break;
				}
			}
		}

		if (p.order.empty())
		{
			*why = "no record of the module file names a module with a shape";
			return false;
		}

		p.deleteOld = false;   // FUN_004c10c0(i, 0, 0), 0x4c8f08-0x4c8f0d
		p.distance = visibleDistance;
		p.open = open;
		p.started = true;
		p.worker = std::thread([&p]() { p.Work(); });
		return true;
	}

	bool AutoLinkRunner::Running() const
	{
		return parts && parts->started && !parts->ended;
	}

	void AutoLinkRunner::Cancel()
	{
		if (parts)
		{
			parts->cancel.store(true);
		}
	}

	void AutoLinkRunner::Step(Frame *out)
	{
		if (!out || !Running())
		{
			return;
		}

		Parts &p = *parts;
		std::deque<Parts::Message> got;
		std::deque<std::vector<uint32_t> > pictures;

		{
			std::lock_guard<std::mutex> hold(p.lock);

			got.swap(p.queue);
			pictures.swap(p.shownPictures);
		}

		for (Parts::Message &message : got)
		{
			if (message.kind == Parts::Message::Kind::Begun)
			{
				// FUN_004c10c0(i, deleteOld, 0): i's tree from its VMDARRAY, or empty with deleteOld; changed put on (0x4c14df). A module this run made current before (Load from file
				// names one twice) had its tree written when it was left: it is read back from those items (its branches all closed again, as a tree read from the file is).
				int32_t const object = p.records.objectIndexOf[(size_t)message.record];
				std::unique_ptr<LinkTree> tree(new LinkTree(object));
				std::vector<LinkItem> items;
				std::map<int32_t, std::unique_ptr<LinkTree> >::const_iterator const before = p.trees.find(object);
				bool const have = before != p.trees.end() ? (items = before->second->Flatten(), true) : (!p.deleteOld && ReadModuleLinks(p.open->Level(), object, &items));

				if (have)
				{
					std::set<int32_t> const &modules = p.records.scene.moduleNumbers;

					tree->Load(items.data(), (int)items.size(), [&modules](int32_t index) { return modules.count(index) != 0; });
				}

				tree->MarkChanged();
				p.tree = tree.get();
				p.trees[object] = std::move(tree);
				p.writer = AutoLinkTreeWriter();
				p.counts.modules++;
				p.counts.lastObject = object;
				out->counters.push_back(message.record);   // 0x4c8a0e-0x4c8a20: the counter, only once it was made current
			}
			else if (message.kind == Parts::Message::Kind::Events)
			{
				p.counts.pictures += message.pictures;

				for (AutoLinkEvent const &event : message.events)
				{
					p.counts.badPictures += event.badPixel && !event.end ? 1 : 0;
					p.writer.Apply(p.tree, event, p.records.objectIndexOf, &p.counts.linksAdded);
				}
			}
			else
			{
				p.finished = true;
				p.stoppedWell = message.ok;
				p.stopWhy = message.why;
			}
		}

		for (std::vector<uint32_t> &picture : pictures)
		{
			out->pictures.push_back(std::move(picture));
		}

		out->finished = p.finished;
	}

	AutoLinkResult AutoLinkRunner::End(OpenLevelFile *open, AutoLinkRunCounts *counts, std::string *why)
	{
		std::string ignored;

		why = why ? why : &ignored;

		if (!parts || !parts->started || parts->ended)
		{
			*why = "no run is on";
			return AutoLinkResult::NothingOpen;
		}

		Parts &p = *parts;

		if (!p.finished)
		{
			p.cancel.store(true);
		}

		if (p.worker.joinable())
		{
			p.worker.join();
		}

		// what the worker posted after the frame's last step
		Frame rest;

		Step(&rest);
		p.ended = true;

		std::string writeWhy;
		AutoLinkResult const result = AutoLink_WriteTrees(open ? open : p.open, p.trees, &p.counts.written, &p.counts.skipped, &writeWhy);

		if (counts)
		{
			*counts = p.counts;
		}

		*why = result != AutoLinkResult::Done ? writeWhy : p.stopWhy;
		return result;
	}
}
