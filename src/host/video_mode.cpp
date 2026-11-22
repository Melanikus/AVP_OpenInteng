// video_mode.cpp: the owner's Video mode's engine (video_mode.h says what and why). Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// ONE WORKER, ONE JOB AT A TIME. A job (a load, a read again for another fit, or a film made for Preview or Save) runs on its own thread with copies of what it needs, and
// says how far it is in an atomic; the editor's thread takes its end in VideoMode_Frame and only then touches what it made. The editor's thread never waits on a job but
// when the mode is left (the job is told to stop first: the read's progress answers false, VideoImportResult::Stopped). The encoder is called by one job at a time.
#include "video_mode.h"

#include <stdio.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>

#include <windows.h>
#include <commdlg.h>

#include "dialogs.h"
#include "editor_shared.h"
#include "fmv_files.h"
#include "message_numbers.h"
#include "smk_encoder.h"
#include "smk_header.h"
#include "video_drop.h"
#include "video_make.h"

namespace
{
	enum class JobKind
	{
		Load,      // the file read, judged, its first still made, then the clip read in its first fit
		Refit,     // the held file read again in another fit
		Make,      // the clip cut, levelled and encoded: for Preview or for Save
	};

	// What a job is given and what it gives back. The editor's thread writes the inputs before the job starts and reads the outputs only once `done` is set
	struct Job
	{
		JobKind kind = JobKind::Load;
		std::atomic<int32_t> percent{ 0 };
		std::atomic<bool> stop{ false };
		std::atomic<bool> done{ false };

		// in
		std::wstring path;                        // Load
		std::shared_ptr<std::vector<uint8_t> const> bytes;   // Refit, and Load's out
		wchar_t const *contentType = nullptr;
		host::VideoImportFit fit = host::VideoImportFit::Fit;
		host::VideoImportClip clip;               // Make: a copy of the frames to make (already cut)
		double volumeDb = 0.0;
		int32_t saveNumber = -1;                  // Make: -1 Preview, else the message number to save as
		bool stockFilm = false;                   // Make: that number is one of Rebellion's own films no level uses (its words say so)

		// out
		host::VideoImportResult result = host::VideoImportResult::BadCall;
		std::string why;
		host::VideoImportInfo info;               // Load
		host::VideoImportClip read;               // Load, Refit
		std::vector<uint8_t> still;               // Load
		uint32_t stillWidth = 0, stillHeight = 0;
		std::vector<uint8_t> smk;                 // Make
	};

	host::VideoModeView view;
	std::string sayWords;   // the status line's words (statusWords copies them)

	// the video held: its file's bytes (kept, so another fit reads it again), its probe, and its clip in the fit now shown
	std::shared_ptr<std::vector<uint8_t> const> heldBytes;
	wchar_t const *heldType = nullptr;
	host::VideoImportInfo heldInfo;
	host::VideoImportClip heldClip;

	std::unique_ptr<Job> job;
	std::thread worker;
	host::FmvPreviewPlace previewPlace;
	bool previewStarted = false;   // the preview that is on is this mode's own (started by Preview's end; FmvPreview_On says whether it still is)

	// AT THE PROGRAM'S END a job still going is told to stop and waited for: a std::thread still running when it is destroyed ends the program (std::terminate).
	// Declared after the worker and the job, so it goes first
	struct WorkerEnd
	{
		~WorkerEnd()
		{
			if (job)
			{
				job->stop = true;
			}

			if (worker.joinable())
			{
				worker.join();
			}
		}
	} workerEnd;

	void Say(std::string const &words)
	{
		sayWords = words;
		statusWords = sayWords.c_str();
	}

	// a length as the film's own words say one ("6 s", "3.5 s"), so the clip and the film made from it say it the same way
	std::string Seconds(double seconds)
	{
		return model::SmkHeader_SecondsWords(seconds);
	}

	double LengthSeconds()
	{
		return (double)heldClip.frameCount / host::kVideoImportFramesPerSecond;
	}

	// The view's words from what is held: the rectangle, the cut, the volume
	void Describe()
	{
		view.fitWords = host::VideoImport_FitWords(heldInfo.width, heldInfo.height, heldInfo.parWidth, heldInfo.parHeight, view.fit);

		if (!host::VideoImport_SourceRect(heldInfo.width, heldInfo.height, heldInfo.parWidth, heldInfo.parHeight, view.fit, view.sourceRect))
		{
			view.sourceRect[0] = view.sourceRect[1] = view.sourceRect[2] = view.sourceRect[3] = 0.0;
		}

		int32_t const from = host::VideoMake_FrameAt(view.fromSeconds, heldClip.frameCount);
		int32_t const to = host::VideoMake_FrameAt(view.toSeconds, heldClip.frameCount);

		view.cutWords = "from " + Seconds(view.fromSeconds) + " to " + Seconds(view.toSeconds) + ": " + Seconds((double)(to - from) / host::kVideoImportFramesPerSecond) + ", " +
			std::to_string((long long)(to - from)) + " frames";
		view.volumeWords = host::VideoMake_VolumeWords(false, view.volumeDb);
	}

	// THE JOB'S WORK, on the worker (WorkSafely below runs it)
	void WorkOn(Job *j)
	{
		auto const progress = [j](int32_t percent) { j->percent = percent; return !j->stop.load(); };

		if (j->kind == JobKind::Load)
		{
			std::vector<uint8_t> bytes;

			j->result = host::VideoImport_ReadFile(j->path, &bytes, &j->why);

			if (j->result == host::VideoImportResult::Done)
			{
				j->contentType = host::VideoImport_ContentType(j->path);
				j->result = host::VideoImport_Probe(bytes, j->contentType, &j->info, &j->why);
			}

			if (j->result == host::VideoImportResult::Done)
			{
				// a picture not of 128 : 96's shape starts on Fit (the whole picture, black bars), the owner's first answer in mp4-to-smk
				j->fit = host::VideoImportFit::Fit;
				j->result = host::VideoImport_Still(bytes, j->contentType, 0, (uint32_t)host::kVideoModeMostStillSide, &j->still, &j->stillWidth, &j->stillHeight, &j->why);
			}

			if (j->result == host::VideoImportResult::Done)
			{
				j->result = host::VideoImport_ReadBytes(bytes, j->contentType, j->fit, &j->read, &j->why, host::kVideoImportMostSeconds, progress);
			}

			j->bytes = std::make_shared<std::vector<uint8_t> const>(std::move(bytes));
		}
		else if (j->kind == JobKind::Refit)
		{
			j->result = host::VideoImport_ReadBytes(*j->bytes, j->contentType, j->fit, &j->read, &j->why, host::kVideoImportMostSeconds, progress);
		}
		else
		{
			// the film: the level and volume, then the encoder linked in (one call: half way when it starts, all when it ends)
			std::string levelWords;
			host::SmkEncoder encoder;

			host::VideoMake_Level(&j->clip, host::kVideoMakeStockLevel, j->volumeDb, &levelWords);
			j->percent = 50;

			if (j->stop)
			{
				j->result = host::VideoImportResult::Stopped;
			}
			else if (!host::SmkEncoder_BuiltIn(&encoder) || !host::SmkEncoder_Encode(encoder, j->clip, &j->smk, &j->why))
			{
				j->result = host::VideoImportResult::BadCall;
				j->why = "Not made: the encoder refused it" + (j->why.empty() ? std::string() : ": " + j->why);
			}
			else
			{
				j->result = host::VideoImportResult::Done;
				j->percent = 100;
			}
		}

	}

	// A job's work, with nothing it throws let out of its thread: a video too big for this 32-bit program's memory (std::bad_alloc, the one thing the read and
	// the encoder throw) would otherwise end the editor (an exception leaving a thread is std::terminate). It ends as a refusal in words instead
	void Work(Job *j)
	{
		try
		{
			WorkOn(j);
		}
		catch (std::exception const &)
		{
			j->read = host::VideoImportClip();
			j->clip = host::VideoImportClip();
			j->smk.clear();
			j->still.clear();
			j->result = host::VideoImportResult::CantRead;
			j->why = "there is not the memory for this video in the editor (a shorter video, or mp4-to-smk's own limits)";
		}

		j->done = true;
	}

	bool Busy()
	{
		return job != nullptr;
	}

	// A job started on its worker; false (said) when Windows gives no thread for it (std::thread throws then: caught here, so it never leaves the window's own handler)
	bool Start(std::unique_ptr<Job> next)
	{
		host::VideoModePhase const before = view.phase;

		job = std::move(next);
		view.percent = 0;
		view.phase = job->kind == JobKind::Make ? host::VideoModePhase::Making : host::VideoModePhase::Loading;

		try
		{
			worker = std::thread(Work, job.get());
		}
		catch (std::exception const &)
		{
			job.reset();
			view.phase = before;
			Say("Not started: Windows gave the editor no thread for it");
			return false;
		}

		return true;
	}

	// The game folder's FMVs folder, and a film's name there as the status line writes it
	std::string FmvsFolder()
	{
		return host::FmvFiles_Folder(theGameFolder);
	}

	std::wstring Wide(std::string const &narrow)
	{
		int const letters = MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
		std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

		if (letters > 0)
		{
			MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), &wide[0], letters);
		}

		return wide;
	}

	// The time now, for a backup's name: "20261122-131500"
	std::wstring Stamp()
	{
		SYSTEMTIME now;
		wchar_t text[32];

		GetLocalTime(&now);
		swprintf(text, sizeof(text) / sizeof(text[0]), L"%04u%02u%02u-%02u%02u%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
		return text;
	}

	// The made film's own line ("SMK2 128 x 96, 45 frames at 15 fps (3 s), 22050 Hz 8-bit mono"), from its header
	std::string FilmWords(std::vector<uint8_t> const &smk)
	{
		model::SmkHeader header;

		return model::SmkHeader_Read(smk.data(), smk.size(), &header) == model::SmkRead::Read ? model::SmkHeader_Words(header) : std::string("a film the editor can't read back");
	}

	// THE JOB'S END, on the editor's thread
	void Finish()
	{
		worker.join();

		std::unique_ptr<Job> const ended = std::move(job);

		if (ended->kind == JobKind::Load)
		{
			if (ended->result != host::VideoImportResult::Done)
			{
				view.phase = heldClip.frameCount > 0 ? host::VideoModePhase::Loaded : host::VideoModePhase::Empty;
				Say(ended->result == host::VideoImportResult::Stopped ? "Not loaded: stopped" :
					"Not loaded: " + (ended->why.empty() ? std::string(host::VideoImport_Words(ended->result)) : ended->why));
				return;
			}

			size_t const slash = ended->path.find_last_of(L"\\/");

			heldBytes = ended->bytes;
			heldType = ended->contentType;
			heldInfo = ended->info;
			heldClip = std::move(ended->read);
			view.name = host::VideoDrop_Words(slash == std::wstring::npos ? ended->path : ended->path.substr(slash + 1));
			view.sourceWidth = heldInfo.width;
			view.sourceHeight = heldInfo.height;
			// (a sound there but not taken, media_parsers.h: the video comes in silent, and the status line says why)
			view.sourceWords = view.name + ": " + std::to_string(heldInfo.width) + " x " + std::to_string(heldInfo.height) + ", " + Seconds(LengthSeconds()) + ", " +
				(heldInfo.hasSound ? "with sound" : heldInfo.soundLeftOut.empty() ? "no sound" : "silent: " + heldInfo.soundLeftOut);
			view.still = std::move(ended->still);
			view.stillWidth = ended->stillWidth;
			view.stillHeight = ended->stillHeight;
			view.fit = ended->fit;
			view.fromSeconds = 0.0;
			view.toSeconds = LengthSeconds();
			view.volumeDb = 0.0;
			view.loadedClips++;
			view.phase = host::VideoModePhase::Loaded;
			Describe();
			Say(view.sourceWords);
			return;
		}

		if (ended->kind == JobKind::Refit)
		{
			view.phase = host::VideoModePhase::Loaded;

			if (ended->result != host::VideoImportResult::Done)
			{
				Say("Not changed: " + (ended->why.empty() ? std::string(host::VideoImport_Words(ended->result)) : ended->why));
				return;
			}

			heldClip = std::move(ended->read);
			view.fit = ended->fit;
			Describe();
			Say(view.fitWords);
			return;
		}

		view.phase = host::VideoModePhase::Loaded;

		if (ended->result != host::VideoImportResult::Done)
		{
			Say(ended->result == host::VideoImportResult::Stopped ? "Not made: stopped" : ended->why);
			return;
		}

		std::string const folder = FmvsFolder();

		if (ended->saveNumber < 0)
		{
			// PREVIEW: the film in a scratch place of the editor's own, played by Barry's decoder where the screen says
			wchar_t temp[MAX_PATH + 1] = {};
			DWORD const length = GetTempPathW(MAX_PATH, temp);
			std::wstring const scratch = std::wstring(temp, length) + L"AVP_OpenInteng-video-preview";
			std::string why;

			CreateDirectoryW(scratch.c_str(), nullptr);

			if (length == 0 || !host::Smk_Replace(scratch + L"\\preview.smk", ended->smk, &why))
			{
				Say("Not previewed: the film couldn't be put in its scratch place" + (why.empty() ? std::string() : ": " + why));
				return;
			}

			std::string playWords;
			char narrow[MAX_PATH * 2] = {};

			WideCharToMultiByte(CP_ACP, 0, scratch.c_str(), -1, narrow, (int)sizeof(narrow), nullptr, nullptr);

			// drawn ON THE SCREEN, under every panel, the bar and every dialog (the owner 11-22: Save pressed during a preview opened its form under the film)
			bool const playing = previewPlace && host::FmvPreview_Start(std::string(narrow) + "\\", "preview.smk", layoutSettingsFile, previewPlace, &playWords,
				host::FmvPreviewLayer::UnderThePanels);

			previewStarted = playing;
			Say(playing ? "Previewing what the game will play: " + FilmWords(ended->smk) : "Not previewed: " + playWords);
			return;
		}

		// SAVE: FMVs\message<n>.smk, a film there kept in FMVs\backup\ first (the user said Yes before this job started)
		std::string const name = model::MessageFmvName(ended->saveNumber);
		std::wstring kept;
		std::string why;

		if (folder.empty() || !host::VideoMake_Save(Wide(folder + name), ended->smk, Wide(folder + "backup"), Stamp(), &kept, &why))
		{
			Say("Not saved: FMVs\\" + name + (why.empty() ? std::string() : ": " + why));
			return;
		}

		Say("Saved FMVs\\" + name + ": " + FilmWords(ended->smk) + (kept.empty() ? std::string() : "; the film it replaced is kept in FMVs\\backup") +
			(ended->stockFilm ? "; it took the place of Rebellion's own film, which Steam's \"Verify integrity of game files\" puts back over it" : std::string()));
	}

	// The clip as the cut says, copied for a job (the held clip stays whole)
	bool CutCopy(host::VideoImportClip *out)
	{
		int32_t const from = host::VideoMake_FrameAt(view.fromSeconds, heldClip.frameCount);
		int32_t const to = host::VideoMake_FrameAt(view.toSeconds, heldClip.frameCount);

		*out = heldClip;
		return host::VideoMake_Cut(out, from, to);
	}

	void MakeFilm(int32_t saveNumber, bool stockFilm = false)
	{
		std::unique_ptr<Job> next(new Job);

		next->kind = JobKind::Make;
		next->volumeDb = view.volumeDb;
		next->saveNumber = saveNumber;
		next->stockFilm = stockFilm;

		if (!CutCopy(&next->clip))
		{
			Say("Not made: the cut holds no frame");
			return;
		}

		host::FmvPreview_Stop();
		Start(std::move(next));
	}

	// SAVE: the number from the free list (model::MessageNumbers_FreeList's rows), then the film; a film there is replaced only after a Yes
	void AskTheNumber()
	{
		host::DialogFormSpec spec;
		host::DialogWidget words, box, ok, cancel;
		std::vector<model::MessageNumberInfo> const free = model::MessageNumbers_FreeList();

		spec.title = "Save video";
		// AS WIDE AS ITS WORDS (the owner, 11-22: "Menus are drawing on top of each other"): in inteng's own letters the label is 405 pixels wide, so a window 360
		// wide had it run 58 pixels past its right edge (the state line's dialogpast; acceptance-video-save-dialog-look). 440 holds it with room on both sides;
		// the buttons keep their 20 pixels from the right edge. AND OK AND CANCEL ARE NEVER COVERED: as inteng's own Message form (strategy_screen.cpp: its buttons under
		// the list's reach), they sit below where the open list ends (the box's foot at 54, then 12 rows of 18: 270), so the list opens over the window's empty part only
		spec.width = 440;
		spec.height = 314;
		words.type = host::DialogWidgetType::Label;
		words.text = "Message number (its film is FMVs\\message<number>.smk):";
		words.box = { 12, 14, 416, 16 };
		box.type = host::DialogWidgetType::Combo;
		box.editable = true;
		box.box = { 12, 36, 330, 18 };   // one row tall: a combo's height is its row's, and each row of its open list is as tall (dialog_box.cpp)
		box.listRows = 12;

		for (model::MessageNumberInfo const &one : free)
		{
			box.items.push_back(one.words);
		}

		box.selected = box.items.empty() ? -1 : 0;
		box.text = box.items.empty() ? std::string() : box.items[0];
		ok.type = host::DialogWidgetType::Button;
		ok.text = "Ok";
		ok.box = { 240, 282, 80, 18 };
		ok.answer = host::kDialogOk;
		cancel = ok;
		cancel.text = "Cancel";
		cancel.box = { 340, 282, 80, 18 };
		cancel.answer = host::kDialogEscape;
		spec.widgets = { words, box, ok, cancel };

		host::Dialogs_Form(spec, [](int32_t answer, std::vector<host::DialogWidget> const &widgets)
		{
			if (answer != host::kDialogOk || widgets.size() < 2)
			{
				Say("Not saved");
				return;
			}

			// the number is the row's (or the typed words') first digits
			std::string const typed = widgets[1].selected >= 0 && (size_t)widgets[1].selected < widgets[1].items.size() ? widgets[1].items[(size_t)widgets[1].selected] : widgets[1].text;
			char *end = nullptr;
			long const number = strtol(typed.c_str(), &end, 10);
			model::MessageNumberInfo const info = model::MessageNumbers_Info(end && end != typed.c_str() && number >= -1000000 && number <= 1000000 ? (int32_t)number : -1);

			if (info.kind == model::MessageNumberKind::Refused)
			{
				Say("Not saved: " + info.words);
				return;
			}

			std::string const name = model::MessageFmvName(info.number);
			std::string const folder = FmvsFolder();

			if (!folder.empty() && GetFileAttributesW(Wide(folder + name).c_str()) != INVALID_FILE_ATTRIBUTES)
			{
				int32_t const n = info.number;
				bool const stock = info.kind == model::MessageNumberKind::StockFilm;

				// what the number is, said in the question itself before the Yes (Vasquez's review, 11-22): one of Rebellion's own films no level uses (both facts), a
				// number a stock level plays (that level would play the new film: 103 is stock Derelict's briefing), or a film there and nothing more known
				std::string question = "FMVs\\" + name + " is there already: replace it with this video?";

				if (stock)
				{
					question = "FMVs\\" + name + " is Rebellion's own film (no stock level plays it): replace it with this video? Steam's \"Verify integrity of game files\" will " +
						"later put Rebellion's back over yours";
				}
				else if (info.kind == model::MessageNumberKind::UsedByStock)
				{
					question = "FMVs\\" + name + " is there already, and a stock level plays it: replace it with this video? That level would then play yours";
				}

				host::Dialogs_Confirm(question, [n, name, stock](bool yes)
				{
					if (!yes)
					{
						Say("Not saved: FMVs\\" + name + " kept as it was");
						return;
					}

					MakeFilm(n, stock);
				});
				return;
			}

			if (info.kind == model::MessageNumberKind::UsedByStock)
			{
				Say(info.words);   // said, then saved: a mapper may mean it (the warning is in words, as Bishop's brief asks)
			}

			MakeFilm(info.number, info.kind == model::MessageNumberKind::StockFilm);
		});
	}
}

host::VideoModeView const &host::VideoMode_View()
{
	return view;
}

void host::VideoMode_Frame()
{
	if (job)
	{
		view.percent = job->percent.load();

		if (job->done)
		{
			Finish();
		}
	}
}

void host::VideoMode_LoadingDrawn()
{
	view.loadingFrames++;
}

bool host::VideoMode_Dropped(std::vector<std::wstring> const &paths, size_t listed)
{
	std::wstring path;
	std::string why;

	if (!VideoDrop_Check(paths, listed, &path, &why))
	{
		Say(why);
		return false;
	}

	if (Busy())
	{
		Say("Not taken: the video before it is still being read or made");
		return false;
	}

	std::unique_ptr<Job> next(new Job);

	next->kind = JobKind::Load;
	next->path = path;
	FmvPreview_Stop();

	if (!Start(std::move(next)))
	{
		return false;
	}

	Say("Loading " + VideoDrop_Words(path));
	return true;
}

void host::VideoMode_Browse(void *owner)
{
	// the filter: the one list of endings (video_import.h), as "*.mp4;*.m4v;..."
	std::wstring patterns;

	for (char const *c = VideoImport_EndingsWords(); *c; c++)
	{
		patterns += *c == '.' ? std::wstring(L"*.") : (*c == ',' ? std::wstring(L";") : (*c == ' ' ? std::wstring() : std::wstring(1, (wchar_t)*c)));
	}

	std::wstring filter = L"Videos (" + patterns + L")";

	filter.push_back(L'\0');
	filter += patterns;
	filter.push_back(L'\0');
	filter.push_back(L'\0');

	wchar_t picked[32768] = {};
	OPENFILENAMEW open = {};

	open.lStructSize = sizeof(open);
	open.hwndOwner = (HWND)owner;
	open.lpstrFilter = filter.c_str();
	open.lpstrFile = picked;
	open.nMaxFile = (DWORD)(sizeof(picked) / sizeof(picked[0]));
	open.lpstrTitle = L"Load a video";
	// OFN_NODEREFERENCELINKS (Vasquez's review, the nit): a shortcut picked comes back as itself, never as its target, so the dialog never checks a target on another
	// computer, and the drop's check below refuses the .lnk as no video
	open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT | OFN_NONETWORKBUTTON | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NODEREFERENCELINKS;

	if (!GetOpenFileNameW(&open))
	{
		Say("Load: nothing picked");
		return;
	}

	// the picked file is hostile all the same (a name can be typed in that box): the drop's own check
	VideoMode_Dropped({ std::wstring(picked) }, 1);
}

bool host::VideoMode_Press(char const *control)
{
	std::string const name = control ? control : "";

	// CANCEL: the job told to stop (a read stops at its next picture; a film being encoded at its end); its end is taken in VideoMode_Frame as any other
	if (name == "Cancel")
	{
		if (!Busy())
		{
			Say("Nothing is being read or made");
			return false;
		}

		job->stop = true;
		Say("Stopping");
		return true;
	}

	if (Busy())
	{
		Say("Wait: the video is still being " + std::string(job->kind == JobKind::Make ? "made" : "read"));
		return false;
	}

	if (heldClip.frameCount <= 0)
	{
		Say("Drop a video first (or use Load -> Browse)");   // the empty screen's own words for the menu (kVideoModeEmptyWords, the owner's "Load -> Browse")
		return false;
	}

	// STOP: the preview playing stopped at once (its button reads Stop while it plays: the owner 11-22, "preview stop/start"); the picture under it shows again
	if (name == "Stop")
	{
		if (!VideoMode_Previewing())
		{
			Say("Nothing is being previewed");
			return false;
		}

		FmvPreview_Stop();
		previewStarted = false;
		Say("Preview stopped");
		return true;
	}

	double const step = kVideoModeCutStepSeconds;
	double const tick = 1.0 / kVideoImportFramesPerSecond;
	double const length = LengthSeconds();

	if (name == "Fit" || name == "Crop" || name == "Expand")
	{
		VideoImportFit const fit = name == "Fit" ? VideoImportFit::Fit : (name == "Crop" ? VideoImportFit::Crop : VideoImportFit::Expand);

		if (fit == view.fit)
		{
			Say(view.fitWords);
			return true;
		}

		std::unique_ptr<Job> next(new Job);

		next->kind = JobKind::Refit;
		next->bytes = heldBytes;
		next->contentType = heldType;
		next->fit = fit;
		FmvPreview_Stop();

		VideoImportFit const before = view.fit;

		view.fit = fit;
		Describe();   // the rectangle moves at once; the frames follow the read

		if (!Start(std::move(next)))
		{
			view.fit = before;
			Describe();
			return false;
		}

		return true;
	}

	if (name == "Start-" || name == "Start+")
	{
		double const want = view.fromSeconds + (name == "Start+" ? step : -step);

		view.fromSeconds = want < 0.0 ? 0.0 : (want > view.toSeconds - tick ? view.toSeconds - tick : want);
	}
	else if (name == "End-" || name == "End+")
	{
		double const want = view.toSeconds + (name == "End+" ? step : -step);

		view.toSeconds = want > length ? length : (want < view.fromSeconds + tick ? view.fromSeconds + tick : want);
	}
	else if (name == "Volume-" || name == "Volume+")
	{
		double const want = view.volumeDb + (name == "Volume+" ? kVideoMakeVolumeStepDb : -kVideoMakeVolumeStepDb);

		view.volumeDb = want > kVideoMakeMostVolumeDb ? kVideoMakeMostVolumeDb : (want < -kVideoMakeMostVolumeDb ? -kVideoMakeMostVolumeDb : want);
	}
	else if (name == "Preview")
	{
		MakeFilm(-1);
		Say("Making the preview");
		return true;
	}
	else if (name == "Save")
	{
		AskTheNumber();
		return true;
	}
	else
	{
		Say("Video: no control named " + name);
		return false;
	}

	Describe();
	Say(name.compare(0, 6, "Volume") == 0 ? "Volume: " + view.volumeWords : "Cut: " + view.cutWords);
	return true;
}

bool host::VideoMode_Previewing()
{
	return previewStarted && FmvPreview_On() && !FmvPreview_Ended();
}

void host::VideoMode_SetPreviewPlace(FmvPreviewPlace const &place)
{
	previewPlace = place;
}

void host::VideoMode_Leave()
{
	if (job)
	{
		job->stop = true;
		worker.join();
		job.reset();
	}

	FmvPreview_Stop();
	previewStarted = false;   // (Vasquez's read, 11-22: so no preview of another screen's could ever read as this mode's)
	heldBytes.reset();
	heldType = nullptr;
	heldInfo = VideoImportInfo();
	heldClip = VideoImportClip();

	size_t const loaded = view.loadedClips, drawn = view.loadingFrames;

	view = VideoModeView();
	view.loadedClips = loaded;
	view.loadingFrames = drawn;
}

std::string host::VideoMode_StateWords()
{
	char const *const phase = view.phase == VideoModePhase::Empty ? "empty" : (view.phase == VideoModePhase::Loading ? "loading" :
		(view.phase == VideoModePhase::Making ? "making" : "loaded"));
	std::string words = std::string(phase);

	if (view.phase == VideoModePhase::Loading || view.phase == VideoModePhase::Making)
	{
		words += " " + std::to_string((long long)view.percent) + "%";
	}

	words += "; loadedclips " + std::to_string((unsigned long long)view.loadedClips) + "; loadingframes " + std::to_string((unsigned long long)view.loadingFrames);

	if (heldClip.frameCount > 0)
	{
		words += "; " + view.sourceWords + "; " + view.cutWords + "; volume " + view.volumeWords;
	}

	return words;
}
