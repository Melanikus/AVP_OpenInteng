// check-export-films.js, run IN the tree (its working folder): the check of an EXPORT MAP tree's films (acceptance-export-map-films.txt). Written 2026-11-27 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// IT TAKES NO PATH (Snyk Code's path rule, javascript/PT): the tree is its working folder (the scenario's line: cmd /c cd /d "<the tree>" && node <this>), and
// the level is Invasion. The films' bytes are compared with the data copy's by the scenario's own lines; this checks what the tree holds and what its README
// says (src/host/export_films.h; design-vasquez-export-films.md), for Invasion with the mapper's message50.smk (a real film over Rebellion's film 50),
// message53.smk (a real film; no stock film has 53) and message54.smk (plain words: not a film):
//   FMVs\message50.smk, FMVs\message53.smk   real Smacker films (SMK2 or SMK4), each other bytes, and NO other file in FMVs: not message54.smk (not a film:
//                                           the players' decoder never gets it), none of Rebellion's own, no video screen film
//   README-Invasion.txt                     each film with its size and sha256 (of the file written); a WARNING that message50.smk replaces Rebellion's film 50
//                                           (naming Invasion.RIF); a WARNING that message54.smk is not exported; the video screen FMVs\triggered_plot;
//                                           Rebellion's films left out; the numbers that play no film; no whole path
// Exit 0 when all is so; each line says what was found.
'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const tree = process.cwd();
const kFilms = ['message50.smk', 'message53.smk'];
let failed = 0;

function check(good, what) {
	console.log(`  ${good ? 'ok' : 'FAILED'}  ${what}`);
	if (!good) failed++;
}

const fmvs = path.join(tree, 'FMVs');
const there = fs.existsSync(fmvs) ? fs.readdirSync(fmvs).sort() : [];

check(there.length === kFilms.length && kFilms.every((n) => there.includes(n)), `FMVs holds exactly the mapper's two films: ${there.join(', ') || 'nothing'}`);

const bytes = {};

for (const name of kFilms) {
	const file = path.join(fmvs, name);
	bytes[name] = fs.existsSync(file) ? fs.readFileSync(file) : Buffer.alloc(0);
	const mark = bytes[name].subarray(0, 4).toString('latin1');
	check(mark === 'SMK2' || mark === 'SMK4', `FMVs\\${name} is a Smacker film (${mark || 'nothing'}), ${bytes[name].length} bytes`);
}

check(bytes[kFilms[0]].length > 0 && !bytes[kFilms[0]].equals(bytes[kFilms[1]]), 'the two films are each other bytes');

const readmeFile = path.join(tree, 'README-Invasion.txt');
const readme = fs.existsSync(readmeFile) ? fs.readFileSync(readmeFile, 'latin1') : '';

check(readme.length > 0, 'README-Invasion.txt is there');

for (const name of kFilms) {
	const sum = crypto.createHash('sha256').update(bytes[name]).digest('hex');
	check(readme.includes(`FMVs\\${name}  ${bytes[name].length} bytes  sha256 ${sum}`), `the README lists FMVs\\${name} with the size and sha256 of the file written`);
}

check(/WARNING: FMVs\\message50\.smk isn't Rebellion's film 50: pasting it replaces that stock film [^\r\n]*Invasion\.RIF/.test(readme),
	'a WARNING says message50.smk replaces Rebellion\'s film 50, naming Invasion.RIF');
check(!/WARNING: FMVs\\message53\.smk/.test(readme), 'no warning for message53.smk (no stock film has 53)');
check(/WARNING: FMVs\\message54\.smk is not exported: it isn't a film the game can play/.test(readme), 'a WARNING says message54.smk is not exported, and why');
check(!fs.existsSync(path.join(fmvs, 'message54.smk')), 'and message54.smk is not in the tree');
check(/FMVs\\triggered_plot/.test(readme) && /video screen/i.test(readme), 'the README names the video screen FMVs\\triggered_plot');
check(/Left out too: 2 films of Rebellion's own/.test(readme), 'the README says 2 of Rebellion\'s films were left out (message55.smk and message64.smk, as shipped)');
check(/play no film/i.test(readme), 'the README names the numbers that play no film');
check(!/[A-Za-z]:\\/.test(readme) && !/\\Users\\/i.test(readme), 'the README holds no whole path (it travels with the map)');

console.log(failed ? `check-export-films: ${failed} FAILED` : 'check-export-films: all ok');
process.exit(failed ? 1 : 0);
