// ps-workflow-blocks-test.js: the test of tools/ps-workflow-blocks.js. Written 2026-11-22 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// Each rule about which steps run in PowerShell has a check both ways, each form the YAML reader refuses has one, each thing that becomes a
// SCAN-ERROR has one, and every real workflow and composite action of the repository and of avpx/ is read with no SCAN-ERROR, each script found
// lying exactly on the lines map.json would name. Use, from the repository's root: node tools/ps-workflow-blocks-test.js. It prints "ok" or
// "FAILED" for each check, and gives back 1 if any failed. It writes nothing.
'use strict';

const fs = require('fs');
const path = require('path');
const tool = require('./ps-workflow-blocks.js');

const root = path.join(__dirname, '..');
let failed = 0;

function check(good, what) {
	console.log(`  ${good ? 'ok' : 'FAILED'}  ${what}`);
	failed += good ? 0 : 1;
}

// A workflow made of lines, as a file would have them
const yaml = (...lines) => lines.join('\n') + '\n';

function found(text, isAction = false) {
	try {
		return tool.powerShellOf(text, isAction);
	} catch (e) {
		return { refused: e instanceof tool.Refused ? e.message : 'THREW ' + e.message, blocks: [], errors: [] };
	}
}

const windowsJob = (...steps) => yaml('on: push', 'jobs:', '  build:', '    runs-on: windows-2025-vs2026   # a comment', '    steps:', ...steps);
const linuxJob = (...steps) => yaml('on: push', 'jobs:', '  build:', '    runs-on: ubuntu-24.04', '    steps:', ...steps);

console.log('Which steps run in PowerShell:');
{
	const r = found(windowsJob('      - name: Build', '        run: |', '          $a = 1', '          # a comment line is script', '', '          Write-Host $a', ''));
	check(r.blocks.length === 1 && r.blocks[0].firstLine === 8 && r.blocks[0].script === '$a = 1\n# a comment line is script\n\nWrite-Host $a\n',
		'a Windows job\'s step with no shell: pwsh, its | block whole (a # line and a blank line inside kept, the blank after it not), from line 8');
	check(r.blocks[0] && r.blocks[0].step === 'Build' && r.blocks[0].job === 'build', 'it names its job and step');
}
check(found(linuxJob('      - run: |', '          echo hi')).blocks.length === 0, 'a Linux job\'s step with no shell: bash, not taken');
check(found(linuxJob('      - shell: pwsh', '        run: Get-Item .')).blocks.length === 1, 'a Linux job\'s step with shell: pwsh: taken');
check(found(linuxJob('      - shell: powershell', '        run: Get-Item .')).blocks.length === 1, 'shell: powershell: taken');
check(found(linuxJob('      - shell: "pwsh -command \\". \'{0}\'\\""', '        run: Get-Item .')).blocks.length === 1, 'a pwsh command line of its own as the shell: taken');
check(found(windowsJob('      - shell: bash', '        run: ls')).blocks.length === 0, 'shell: bash on Windows: not taken');
check(found(windowsJob('      - shell: cmd', '        run: dir')).blocks.length === 0, 'shell: cmd on Windows: not taken');
{
	const r = found(windowsJob('      - name: One line', '        run: ./tools/build.ps1 -Out x'));
	check(r.blocks.length === 1 && r.blocks[0].firstLine === 7 && r.blocks[0].script === './tools/build.ps1 -Out x\n', 'a one-line run: taken, on its own line');
}
{
	const r = found(yaml('on: push', 'jobs:', '  a:', '    runs-on: ubuntu-24.04', '    defaults:', '      run:', '        shell: pwsh', '    steps:', '      - run: Get-Date', '      - shell: bash', '        run: date'));
	check(r.blocks.length === 1 && r.blocks[0].script === 'Get-Date\n', 'a job\'s defaults run shell pwsh: its unnamed steps taken, a step naming bash not');
}
check(found(yaml('defaults:', '  run:', '    shell: powershell', 'on: push', 'jobs:', '  a:', '    runs-on: ubuntu-24.04', '    steps:', '      - run: Get-Date')).blocks.length === 1,
	'the workflow\'s defaults run shell: taken');
check(found(yaml('on: push', 'jobs:', '  a:', '    runs-on: windows-2025-vs2026', '    defaults:', '      run:', '        shell: bash', '    steps:', '      - run: ls')).blocks.length === 0,
	'a Windows job whose defaults say bash: not taken');
check(found(yaml('on: push', 'jobs:', '  a:', '    runs-on: [self-hosted, windows]', '    steps:', '      - run: Get-Date')).errors.length === 1,
	'runs-on written as [..]: not read, so its unnamed step is a SCAN-ERROR, never passed over');
check(found(yaml('on: push', 'jobs:', '  a:', '    runs-on:', '      - self-hosted', '      - windows', '    steps:', '      - run: Get-Date')).blocks.length === 1,
	'runs-on as a list holding windows: pwsh');
check(found(yaml('on: push', 'jobs:', '  a:', '    steps:', '    - name: x', '      shell: pwsh', '      run: Get-Date')).blocks.length === 1,
	'steps listed at the same indentation as "steps:"');
check(found(yaml('on: push', 'jobs:', '  a:', '    runs-on: windows-2025-vs2026', '    steps:', '      - uses: actions/checkout@v4', '        with:', '          script: |',
	'            Get-Date')).blocks.length === 0, 'a uses: step (an action): no script of ours, even with a | value in its with:');
check(found(yaml('on: push', 'jobs:', '  call:', '    uses: ./.github/workflows/other.yml')).blocks.length === 0 && found(yaml('on: push', 'jobs:', '  call:',
	'    uses: ./.github/workflows/other.yml')).errors.length === 0, 'a job that calls another workflow: no steps, no error');
check(found(yaml('runs:', '  using: composite', '  steps:', '    - shell: pwsh', '      run: |', '        Get-Date'), true).blocks.length === 1,
	'a composite action\'s pwsh step: taken');
check(found(yaml('runs:', '  using: node20', '  main: index.js'), true).blocks.length === 0, 'a JavaScript action: nothing');
{
	const r = found(windowsJob('      - run: |', '          Get-Item "${{ runner.temp }}\\x" ${{ inputs.a }}:b', '          if (${{', '            github.event_name', '          }} -eq 1) { }',
		'          Write-Host x'));
	check(r.blocks.length === 1 && r.blocks[0].script === 'Get-Item "${GhExpr1}\\x" ${GhExpr2}:b\nif (${GhExpr3}\n\n -eq 1) { }\nWrite-Host x\n' && r.blocks[0].expressions === 3,
		'each ${{ }} becomes ${GhExprN}, braced (so "x:" after it parses), keeping its line breaks so the lines stay in place');
}
{
	const r = found(windowsJob('      - run: |-', '          one', '          two'));
	check(r.blocks.length === 1 && r.blocks[0].script === 'one\ntwo\n', 'a |- block');
}
{
	const r = found(windowsJob('      - run: Write-Host one', '          two'));
	check(r.blocks.length === 1 && r.blocks[0].script === 'Write-Host one two\n' && r.blocks[0].folded === true, 'a plain run: over two lines is read as YAML folds it (one line), and marked folded');
}
check(found(windowsJob('      - run: "Write-Host \\"a: b\\""')).blocks[0].script === 'Write-Host "a: b"\n', 'a double-quoted run:');
check(found(windowsJob("      - run: 'Write-Host ''a'''")).blocks[0].script === "Write-Host 'a'\n", 'a single-quoted run:');

console.log('Forms the YAML reader refuses (the whole file is a SCAN-ERROR):');
const refusedFor = (text, words, what) => {
	const r = found(text);
	check(r.refused !== undefined && r.refused.includes(words), `${what} (${r.refused === undefined ? 'not refused' : r.refused})`);
};
refusedFor(windowsJob('      - run: |', '\t  Get-Date'), 'a tab', 'a tab');
refusedFor(yaml('on: push', 'x: &anchor', '  a: 1', 'jobs:', '  a:', '    steps: []'), 'anchor', 'an anchor');
refusedFor(yaml('on: push', 'y: *anchor', 'jobs:', '  a:', '    steps: []'), 'anchor', 'an alias');
refusedFor(yaml('on: push', '<<: {}', 'jobs: {}'), 'merge key', 'a merge key');
refusedFor(windowsJob('      - run: >', '          Get-Date'), 'folded (>)', 'a folded > block (its lines would run joined)');
refusedFor(yaml('on: push', 'on: pull_request', 'jobs: {}'), 'twice', 'a key twice');
refusedFor(yaml('---', 'on: push', 'jobs: {}'), 'document marker', 'a document marker');
refusedFor(yaml('on: push', 'jobs:', '  a:', '      runs-on: x', '    steps: []'), 'indented more', 'a line indented more than its mapping');
refusedFor(windowsJob('      - run: |', '            Get-Date', '          Get-Item .'), 'less than its first line', 'a | block line indented less than its first line');
refusedFor(windowsJob('      - run: "Write-Host'), 'doesn\'t close', 'a double-quoted value that doesn\'t close');
refusedFor(windowsJob('      - run: |9x'), 'block header', 'a block header it can\'t read');
refusedFor(yaml('  on: push'), 'first line is indented', 'a first line that is indented');
refusedFor(yaml('on: push', 'jobs: [a]'), 'no jobs: mapping', 'jobs that are not a mapping');

console.log('Steps it can\'t read are SCAN-ERRORs, never passed over:');
check(found(yaml('on: push', 'jobs:', '  a:', '    runs-on: ${{ matrix.os }}', '    steps:', '      - run: Get-Date')).errors.length === 1,
	'runs-on an expression, a step with no shell');
check(found(windowsJob('      - shell: ${{ matrix.shell }}', '        run: Get-Date')).errors.length === 1, 'a shell that is an expression');
check(found(windowsJob('      - run: Write-Host ${{ never closed')).errors.length === 1, 'a ${{ that never closes');
check(found(windowsJob('      - just a word')).errors.length === 1, 'a step that is not a mapping');

console.log('Every real workflow and composite action, read with no SCAN-ERROR, each script on the lines map.json names:');
for (const base of ['', 'avpx']) {
	const github = path.join(root, base, '.github');
	const files = [];

	for (const name of (fs.existsSync(path.join(github, 'workflows')) ? fs.readdirSync(path.join(github, 'workflows')) : [])) {
		if (/\.ya?ml$/.test(name)) {
			files.push({ name: path.posix.join(base, '.github/workflows', name), full: path.join(github, 'workflows', name), action: false });
		}
	}

	for (const folder of (fs.existsSync(path.join(github, 'actions')) ? fs.readdirSync(path.join(github, 'actions')) : [])) {
		for (const name of ['action.yml', 'action.yaml']) {
			if (fs.existsSync(path.join(github, 'actions', folder, name))) {
				files.push({ name: path.posix.join(base, '.github/actions', folder, name), full: path.join(github, 'actions', folder, name), action: true });
			}
		}
	}

	check(files.length > 0, `${base || 'the repository'}: ${files.length} workflow files found`);

	for (const file of files) {
		const text = fs.readFileSync(file.full, 'utf8');
		const r = found(text, file.action);
		const lines = text.replace(/^\uFEFF/, '').split(/\r?\n/);

		// each | block's lines, ${{ }} aside, are the file's own lines from firstLine on, with the block's indentation taken off; a one-line
		// run: is on its own line, after "run:"
		const placed = r.blocks.every((block) => block.folded || block.script.replace(/\n$/, '').split('\n').every((line, i) => {
			const there = lines[block.firstLine - 1 + i];

			if (there === undefined) {
				return false;
			}

			if (line.includes('${GhExpr')) {
				return true;
			}

			// (a quoted one-line value is unescaped, so it is found on its run: line but not letter for letter)
			return block.kind === 'line' ? /^\s*(- )?run:\s/.test(there) && (there.includes(line) || /run:\s+["']/.test(there)) : there.trim() === line.trim();
		}));

		check(r.refused === undefined && r.errors.length === 0 && placed,
			`${file.name}: ${r.refused ? 'REFUSED ' + r.refused : `${r.blocks.length} PowerShell steps, ${r.errors.length} SCAN-ERRORs`}${placed ? '' : ', a script NOT on its lines'}`);
	}
}

console.log(failed ? `SOME CHECKS FAILED: ${failed}` : 'All checks passed');
process.exit(failed ? 1 : 0);
