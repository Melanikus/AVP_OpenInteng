// make-wavs.js <name>: one of the WAV files the sound import's acceptances drop (acceptance-sound-import-*.txt, acceptance-export-map.txt), its bytes written to
// standard output. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// IT TAKES NO PATH (Snyk Code's path rule, javascript/PT; as message-fmv\fmv-hostile.js): <name> only picks one of the files below, and the scenario's own line
// writes them where it says, with an inline line (code in a scenario's line is no file Snyk scans):
//   make-level node -e "... require(m).WavBytes(n) ..." "{scenariodir}sound-import\make-wavs.js" {datadir} import/zzboom.wav=zzboom.wav
// require()d, it gives WavBytes(name), so a check can make the very same bytes again (check-export.js).
// The repository holds no sound, so each run makes its files fresh. Every file is a short tone. Each "bad" file differs from a good one in ONE thing only, so
// each refusal is for the reason its name says:
//   zzboom.wav        good: plain PCM, 16 bits, mono, 22050 Hz, 0.25 s
//   zzstereo.wav      good: the same, stereo
//   alienpres11-8.wav good, but named like one of Invasion's own stock sounds (its pack, Snd17.FFL, holds sound\Env\Invasion\alienpres11-8.wav)
//   zzboom-other.wav  good, a lower tone: dropped from import2 as zzboom.wav, the same name as zzboom.wav but other bytes
//   zz24.wav          24 bits a sample
//   zzadpcm.wav       format tag 2 (not plain PCM), everything else as zzboom.wav
//   zzlistfirst.wav   a LIST chunk before the fmt chunk
//   zzfmt20.wav       a fmt chunk of 20 bytes (the game reads only 16, or 18 with cbSize 0)
//   zzshort.wav       its data chunk says more bytes than the file holds
//   zznotwav.wav      not a RIFF WAVE file at all (text)
//   zzhuge.wav        good, but 64 MB and two bytes of sound (the game's limit is 64 MB; two, so it is still a whole number of 2-byte blocks)
//   zz..dots.wav      good, but its name holds ".." (the game's name rule refuses it)

// A RIFF WAVE file: the chunks given, in order, each as [id, bytes]
function riff(chunks)
{
	const parts = [];

	for (const [id, body] of chunks)
	{
		const head = Buffer.alloc(8);

		head.write(id, 0, 'latin1');
		head.writeUInt32LE(body.length, 4);
		parts.push(head, body);

		// a chunk of an odd length is followed by one pad byte
		if (body.length % 2)
		{
			parts.push(Buffer.alloc(1));
		}
	}

	const all = Buffer.concat(parts);
	const top = Buffer.alloc(12);

	top.write('RIFF', 0, 'latin1');
	top.writeUInt32LE(all.length + 4, 4);
	top.write('WAVE', 8, 'latin1');
	return Buffer.concat([top, all]);
}

// The fmt chunk: tag, channels, rate, bits (and its own length: 16, or more with zero bytes after)
function fmt(tag, channels, rate, bits, length = 16)
{
	const b = Buffer.alloc(length);
	const align = channels * Math.ceil(bits / 8);

	b.writeUInt16LE(tag, 0);
	b.writeUInt16LE(channels, 2);
	b.writeUInt32LE(rate, 4);
	b.writeUInt32LE(rate * align, 8);
	b.writeUInt16LE(align, 12);
	b.writeUInt16LE(bits, 14);
	return b;
}

// A tone of `seconds` at `hz`, as samples of `bits` (16 or 24) on `channels`
function tone(seconds, hz, bits, channels, rate = 22050)
{
	const count = Math.round(seconds * rate);
	const size = Math.ceil(bits / 8);
	const b = Buffer.alloc(count * size * channels);

	for (let i = 0; i < count; i++)
	{
		const v = Math.round(Math.sin(2 * Math.PI * hz * i / rate) * 0.3 * (bits === 24 ? 8388607 : 32767));

		for (let c = 0; c < channels; c++)
		{
			const at = (i * channels + c) * size;

			if (bits === 24)
			{
				b.writeIntLE(v, at, 3);
			}
			else
			{
				b.writeInt16LE(v, at);
			}
		}
	}

	return b;
}

const good = (hz = 440, channels = 1) => riff([['fmt ', fmt(1, channels, 22050, 16)], ['data', tone(0.25, hz, 16, channels)]]);

// Each file, made only when asked for (zzhuge.wav is 64 MB)
const makers = {
	'zzboom.wav': () => good(),
	'zzstereo.wav': () => good(440, 2),
	'alienpres11-8.wav': () => good(660),
	'zzboom-other.wav': () => good(220),
	'zz24.wav': () => riff([['fmt ', fmt(1, 1, 22050, 24)], ['data', tone(0.25, 440, 24, 1)]]),
	'zzadpcm.wav': () => riff([['fmt ', fmt(2, 1, 22050, 16)], ['data', tone(0.25, 440, 16, 1)]]),
	'zzlistfirst.wav': () => riff([['LIST', Buffer.from('INFOISFT\x04\x00\x00\x00test', 'latin1')], ['fmt ', fmt(1, 1, 22050, 16)], ['data', tone(0.25, 440, 16, 1)]]),
	'zzfmt20.wav': () => riff([['fmt ', fmt(1, 1, 22050, 16, 20)], ['data', tone(0.25, 440, 16, 1)]]),
	'zznotwav.wav': () => Buffer.from('This is not a sound.\r\n', 'latin1'),
	'zz..dots.wav': () => good(),
	// a good file whose data chunk claims 1,000 bytes more than are there
	'zzshort.wav': () =>
	{
		const b = good();
		const at = b.indexOf('data', 12, 'latin1');

		b.writeUInt32LE(b.readUInt32LE(at + 4) + 1000, at + 4);
		return b;
	},
	// 64 MB and two bytes of silence after a good fmt (the game's own limit, kMaxWavDataBytes, is 64 MB)
	'zzhuge.wav': () => riff([['fmt ', fmt(1, 1, 22050, 16)], ['data', Buffer.alloc(64 * 1024 * 1024 + 2)]]),
};

// The bytes of one of the files above, by its name; null for a name that isn't one
function WavBytes(name)
{
	return Object.prototype.hasOwnProperty.call(makers, name) ? makers[name]() : null;
}

module.exports = { WavBytes };

if (require.main === module)
{
	const bytes = WavBytes(process.argv[2]);

	if (!bytes)
	{
		process.stderr.write(`usage: node make-wavs.js <name> > <file>; the names: ${Object.keys(makers).join(' ')}\n`);
		process.exit(2);
	}

	process.stdout.write(bytes);
}
