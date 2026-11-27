// check-export.js, run IN the tree (its working folder): the check of an EXPORT MAP tree (acceptance-export-map.txt). Written 2026-11-27 by Melanikus (Dietrich)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// IT TAKES NO PATH (Snyk Code's path rule, javascript/PT): the tree is its working folder (the scenario's line: cmd /c cd /d "<the tree>" && node <this>), the
// level is Invasion, and the WAV the tree must hold is made again here by make-wavs.js, byte for byte as the scenario made it.
// What Export map must have written in the tree (AvP-docs director/design-dietrich-sound-import-export-map.md, section 5; the owner's "Loose WAVs,
// paste-and-play", 2026-11-27), for a level whose one placed sound names zzboom.wav, loaded into its stock folder env\Invasion, with zzstereo.wav loaded too
// but named by no sound:
//   avp_rifs\Custom\Invasion.RIF     a level file (REBCRIF1 packed or REBINFF2 plain, the two the game opens)
//   sound\env\Invasion\zzboom.wav    the level's own sound, LOOSE where the game looks for it, its bytes make-wavs.js's zzboom.wav; not zzstereo.wav (no sound
//                                    names it), not a stock sound (every install has them)
//   README-Invasion.txt              names the folder from Documents on (no whole path: it travels with the map), says to paste it, and names zzboom.wav
//                                    with its sha256
//   and NO fastfile folder: no pack, no ffinfo.txt line (the owner's "So cancel packing", 11-27)
// Exit 0 when all is so; each line says what was found.
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { WavBytes } = require('./make-wavs.js');

const level = 'Invasion';
const tree = process.cwd();
let failed = 0;

function check(good, what)
{
	console.log(`  ${good ? 'ok' : 'FAILED'}  ${what}`);
	failed += good ? 0 : 1;
}

function readOrNothing(file, how)
{
	return fs.existsSync(file) ? fs.readFileSync(file, how) : null;
}

const loaded = WavBytes('zzboom.wav');

// THE LEVEL
const rif = readOrNothing(path.join(tree, 'avp_rifs', 'Custom', `${level}.RIF`));

check(rif && rif.length > 1000 && ['REBCRIF1', 'REBINFF2'].includes(rif.toString('latin1', 0, 8)), `the level avp_rifs\\Custom\\${level}.RIF (${rif ? rif.length : 0} bytes)`);

// THE SOUND, loose
const sound = readOrNothing(path.join(tree, 'sound', 'env', 'Invasion', 'zzboom.wav'));

check(sound && sound.equals(loaded), `sound\\env\\Invasion\\zzboom.wav is there, the WAV loaded, byte for byte (${sound ? sound.length : 0} bytes)`);
check(!fs.existsSync(path.join(tree, 'sound', 'env', 'Invasion', 'zzstereo.wav')), 'sound\\env\\Invasion\\zzstereo.wav is not (no sound names it)');
check(!fs.existsSync(path.join(tree, 'fastfile')), 'no fastfile folder (no pack and no ffinfo.txt line: the owner\'s "So cancel packing")');

// THE README
const readme = readOrNothing(path.join(tree, `README-${level}.txt`), 'latin1') || '';
const sha = crypto.createHash('sha256').update(loaded).digest('hex');

check(readme.includes(`AVP_OpenInteng\\Export\\${level}`), `README-${level}.txt names the folder it is in (AVP_OpenInteng\\Export\\${level})`);
check(!/[A-Za-z]:\\/.test(readme) && !/\\Users\\/i.test(readme), `README-${level}.txt holds no whole path (it travels with the map: the mapper's Windows name)`);
check(!fs.existsSync(path.join(tree, 'README.txt')), 'no plain README.txt (pasted beside other maps\' it would be written over)');
check(readme.includes('copy everything in this folder into the Aliens versus Predator Classic folder'), `README-${level}.txt says to paste the folder into the AvP folder`);
check(readme.includes('sound\\env\\Invasion\\zzboom.wav') && readme.toLowerCase().includes(sha), `README-${level}.txt names sound\\env\\Invasion\\zzboom.wav with its sha256 (${sha.slice(0, 16)}...)`);
check(!readme.includes('zzstereo.wav'), `README-${level}.txt doesn't name zzstereo.wav (no sound plays it)`);

console.log(failed ? `${failed} FAILED` : 'all ok');
process.exit(failed ? 1 : 0);
