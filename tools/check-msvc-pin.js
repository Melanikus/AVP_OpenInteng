// check-msvc-pin.js: THE ONE COMPILER, held. Every program this repository ships is built with AvPx's one MSVC (avpx/win32/msvc-pin.props says why:
// alpha.3 shipped what a newer runner compiler made, never what had been scanned). Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng, Bishop's
// order after alpha.3 (Tao 2: a fix nothing enforces comes undone). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It refuses, naming the file and why:
//   1. a project in win32/VS2026 that doesn't import $(MSBuildThisFileDirectory)..\..\avpx\win32\msvc-pin.props exactly once, with no condition, after
//      Microsoft.Cpp.Default.props and before Microsoft.Cpp.props (a developer prompt clears a value set before the first; the second takes the newest
//      installed MSVC when it is empty);
//   2. a GitHub job on Windows whose first building step comes before the pin is installed. Building: MSBuild, "vcpkg.exe ... install", or a script that
//      builds (AvPx's build-and-package.ps1, tools/run-unit-tests.ps1). Installing: avpx/tools/install-msvc-pin.ps1, or one of those two scripts, which
//      install it themselves on GitHub's runner before they build anything.
// The workflows are read line by line, as they are written in this repository (jobs two spaces in, steps under them): it is a check of our own files,
// not a YAML reader.
//
// Run from the repository's root: node tools/check-msvc-pin.js [folder]. It answers 0 when everything holds, 1 (and why) when not. Its refusals are
// shown by tools/check-msvc-pin-test.js. With no folder it reads the one it is run from; a folder named on the command line comes from outside the
// tool, so it is held inside the one it is run from, as the editor's other tools hold theirs (tools/tree-path.js).
'use strict';
const fs = require('fs');
const path = require('path');
const { insideTree } = require('./tree-path.js');

const root = process.argv[2] ? insideTree(process.argv[2], 'The folder to check') : process.cwd();
const problems = [];
const pinImport = '<Import Project="$(MSBuildThisFileDirectory)..\\..\\avpx\\win32\\msvc-pin.props" />';

// 1. The projects
const projects = path.join(root, 'win32', 'VS2026');
const projectNames = fs.existsSync(projects) ? fs.readdirSync(projects).filter(n => /\.vcxproj$/i.test(n)).sort() : [];
if (projectNames.length === 0) problems.push('win32/VS2026 holds no project');
for (const name of projectNames) {
	const lines = fs.readFileSync(path.join(projects, name), 'utf8').split(/\r?\n/);
	const at = test => lines.findIndex(test);
	const pins = lines.map((l, i) => [l, i]).filter(([l]) => /msvc-pin\.props/.test(l) && /<Import\b/.test(l));
	const defaults = at(l => /<Import\s+Project="\$\(VCTargetsPath\)\\Microsoft\.Cpp\.Default\.props"\s*\/>/.test(l));
	const props = at(l => /<Import\s+Project="\$\(VCTargetsPath\)\\Microsoft\.Cpp\.props"\s*\/>/.test(l));
	const where = 'win32/VS2026/' + name;
	if (pins.length !== 1) { problems.push(where + ': imports msvc-pin.props ' + pins.length + ' times, not once'); continue; }
	if (pins[0][0].trim() !== pinImport) { problems.push(where + ': its msvc-pin.props import is not exactly ' + pinImport + ' (no condition, from this folder)'); continue; }
	if (defaults < 0 || props < 0) { problems.push(where + ': no Microsoft.Cpp.Default.props or Microsoft.Cpp.props import to place it between'); continue; }
	if (!(defaults < pins[0][1] && pins[0][1] < props)) problems.push(where + ': msvc-pin.props must be imported after Microsoft.Cpp.Default.props and before Microsoft.Cpp.props');
}

// 2. The workflows: each Windows job installs the pin before it builds
const builds = [/&\s*\$msbuild\b/i, /vcpkg\.exe['"]?\)?\s+install\b/i, /build-and-package\.ps1/, /run-unit-tests\.ps1/];
const installs = [/install-msvc-pin\.ps1/, /build-and-package\.ps1/, /run-unit-tests\.ps1/];
const workflows = path.join(root, '.github', 'workflows');
for (const file of (fs.existsSync(workflows) ? fs.readdirSync(workflows) : []).filter(n => /\.ya?ml$/.test(n)).sort()) {
	const lines = fs.readFileSync(path.join(workflows, file), 'utf8').split(/\r?\n/);
	const jobsAt = lines.findIndex(l => /^jobs:\s*$/.test(l));
	if (jobsAt < 0) continue;
	// a job: a line two spaces in under jobs:, until the next one
	const starts = [];
	for (let i = jobsAt + 1; i < lines.length; i++) if (/^  [A-Za-z0-9_-]+:\s*$/.test(lines[i])) starts.push(i);
	starts.forEach((start, k) => {
		const job = lines.slice(start, k + 1 < starts.length ? starts[k + 1] : lines.length);
		if (!job.some(l => /^\s+runs-on:\s*windows/.test(l))) return;
		// comments are no steps
		const code = job.map(l => l.replace(/^\s*#.*$/, ''));
		const firstBuild = code.findIndex(l => builds.some(b => b.test(l)));
		if (firstBuild < 0) return;
		const firstInstall = code.findIndex(l => installs.some(s => s.test(l)));
		if (firstInstall < 0 || firstInstall > firstBuild) {
			problems.push('.github/workflows/' + file + ', job ' + lines[start].trim().replace(/:$/, '') + ': it builds at line ' + (start + firstBuild + 1) +
				' before installing the one compiler (avpx/tools/install-msvc-pin.ps1)');
		}
	});
}

if (problems.length) {
	for (const p of problems) console.log('check-msvc-pin: REFUSED: ' + p);
	process.exit(1);
}
console.log('check-msvc-pin: every project imports the one compiler and every Windows job installs it before it builds (' + projectNames.length + ' projects)');
