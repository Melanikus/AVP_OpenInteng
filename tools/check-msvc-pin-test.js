// check-msvc-pin-test.js: the test of tools/check-msvc-pin.js's refusals, each on a made-up tree in a folder of its own that this makes and deletes.
// Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// Run from the repository's root: node tools/check-msvc-pin-test.js. It answers 0 when every check passed.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const script = path.join(__dirname, 'check-msvc-pin.js');
let failed = 0;
function check(good, what) {
	console.log('  ' + (good ? 'ok' : 'FAILED') + '  ' + what);
	if (!good) failed++;
}

const pin = '  <Import Project="$(MSBuildThisFileDirectory)..\\..\\avpx\\win32\\msvc-pin.props" />';
const defaults = '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />';
const props = '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />';
const project = middle => ['<Project>', defaults, '  <PropertyGroup Label="Configuration" />'].concat(middle, [props, '</Project>']).join('\r\n');
const goodProject = project([pin]);
const goodJob = ['jobs:', '  build:', '    runs-on: windows-2025-vs2026', '    steps:', '      - name: Install the one compiler', '        run: ./avpx/tools/install-msvc-pin.ps1',
	'      - name: Build', '        run: |', '          & $msbuild win32\\VS2026\\AVP_OpenInteng.vcxproj'].join('\n');

// One made-up tree: its projects (name -> text) and workflows (name -> text), checked; gives back the answer and what was said
function run(projects, workflows) {
	const root = fs.mkdtempSync(path.join(os.tmpdir(), 'check-msvc-pin-test-'));
	try {
		fs.mkdirSync(path.join(root, 'win32', 'VS2026'), { recursive: true });
		fs.mkdirSync(path.join(root, '.github', 'workflows'), { recursive: true });
		for (const [n, t] of Object.entries(projects)) fs.writeFileSync(path.join(root, 'win32', 'VS2026', n), t);
		for (const [n, t] of Object.entries(workflows)) fs.writeFileSync(path.join(root, '.github', 'workflows', n), t);
		// run from the made-up tree's root, as the check is run from the repository's: it reads the folder it is run from
		const r = spawnSync(process.execPath, [script], { cwd: root, encoding: 'utf8' });
		return { status: r.status, said: (r.stdout || '') + (r.stderr || '') };
	} finally {
		fs.rmSync(root, { recursive: true, force: true });
	}
}

let r = run({ 'a.vcxproj': goodProject, 'b.vcxproj': goodProject }, { 'build.yml': goodJob });
check(r.status === 0, 'the control: two projects importing the pin in place, and a job that installs before it builds, pass. It said: ' + r.said.trim());

const refusals = [
	{ what: 'a project with no pin', projects: { 'a.vcxproj': project([]) }, says: 'imports msvc-pin.props 0 times' },
	{ what: 'a project importing it twice', projects: { 'a.vcxproj': project([pin, pin]) }, says: 'imports msvc-pin.props 2 times' },
	{ what: 'a project importing it under a condition (a tree without the pin would build unpinned)',
		projects: { 'a.vcxproj': project(['  <Import Project="$(MSBuildThisFileDirectory)..\\..\\avpx\\win32\\msvc-pin.props" Condition="Exists(\'x\')" />']) },
		says: 'is not exactly' },
	{ what: 'a project importing it after Microsoft.Cpp.props (the newest MSVC is taken by then)',
		projects: { 'a.vcxproj': ['<Project>', defaults, props, pin, '</Project>'].join('\r\n') }, says: 'must be imported after Microsoft.Cpp.Default.props and before Microsoft.Cpp.props' },
	{ what: 'a project importing it before Microsoft.Cpp.Default.props (a developer prompt clears it)',
		projects: { 'a.vcxproj': ['<Project>', pin, defaults, props, '</Project>'].join('\r\n') }, says: 'must be imported after' },
	{ what: 'a Windows job that builds before it installs', workflows: { 'build.yml': goodJob.replace('./avpx/tools/install-msvc-pin.ps1', 'echo nothing') },
		says: 'job build: it builds at line' },
	{ what: 'a Windows job whose vcpkg comes before the install',
		workflows: { 'build.yml': ['jobs:', '  libs:', '    runs-on: windows-2025-vs2026', '    steps:', "      - run: & (Join-Path $root 'vcpkg.exe') install --triplet x86-windows",
			'      - run: ./avpx/tools/install-msvc-pin.ps1'].join('\n') }, says: 'job libs: it builds at line 5' },
	{ what: 'an install named only in a comment',
		workflows: { 'build.yml': goodJob.replace('        run: ./avpx/tools/install-msvc-pin.ps1', '        # ./avpx/tools/install-msvc-pin.ps1 would go here') }, says: 'before installing' },
];
for (const c of refusals) {
	r = run(c.projects || { 'a.vcxproj': goodProject }, c.workflows || { 'build.yml': goodJob });
	check(r.status === 1 && r.said.includes(c.says), 'refused, saying why: ' + c.what);
}

// A job on Ubuntu, or one that builds nothing, needs no install; a job whose first build is AvPx's own script (it installs first) passes
r = run({ 'a.vcxproj': goodProject }, { 'a.yml': ['jobs:', '  lint:', '    runs-on: ubuntu-24.04', '    steps:', '      - run: & $msbuild x'].join('\n'),
	'b.yml': ['jobs:', '  scan:', '    runs-on: windows-2025-vs2026', '    steps:', '      - run: echo hello'].join('\n'),
	'c.yml': ['jobs:', '  pkg:', '    runs-on: windows-2025-vs2026', '    steps:', '      - run: ./tools/build-and-package.ps1 -OutFolder x', '      - run: & $msbuild y'].join('\n') });
check(r.status === 0, 'an Ubuntu job, a job that builds nothing, and one whose first build is AvPx\'s build-and-package.ps1 (which installs first) pass. It said: ' + r.said.trim());

console.log(failed ? 'SOME CHECKS FAILED' : 'All checks passed');
process.exit(failed ? 1 : 0);
