// See inteng_exe.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "inteng_exe.h"

#include <stdio.h>
#include <string.h>

#include <windows.h>

namespace host
{
	namespace
	{
		// ---- SHA-256 (FIPS 180-4) ----
		uint32_t const kRound[64] =
		{
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
			0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
			0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
			0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
			0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
		};

		uint32_t Turn(uint32_t x, int by)
		{
			return (x >> by) | (x << (32 - by));
		}

		void Block(uint32_t state[8], uint8_t const block[64])
		{
			uint32_t w[64];

			for (int i = 0; i < 16; i++)
			{
				w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) | ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
			}

			for (int i = 16; i < 64; i++)
			{
				uint32_t const s0 = Turn(w[i - 15], 7) ^ Turn(w[i - 15], 18) ^ (w[i - 15] >> 3);
				uint32_t const s1 = Turn(w[i - 2], 17) ^ Turn(w[i - 2], 19) ^ (w[i - 2] >> 10);

				w[i] = w[i - 16] + s0 + w[i - 7] + s1;
			}

			uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];

			for (int i = 0; i < 64; i++)
			{
				uint32_t const t1 = h + (Turn(e, 6) ^ Turn(e, 11) ^ Turn(e, 25)) + ((e & f) ^ (~e & g)) + kRound[i] + w[i];
				uint32_t const t2 = (Turn(a, 2) ^ Turn(a, 13) ^ Turn(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));

				h = g;
				g = f;
				f = e;
				e = d + t1;
				d = c;
				c = b;
				b = a;
				a = t1 + t2;
			}

			state[0] += a;
			state[1] += b;
			state[2] += c;
			state[3] += d;
			state[4] += e;
			state[5] += f;
			state[6] += g;
			state[7] += h;
		}

		uint32_t U32(std::vector<uint8_t> const &b, size_t at)
		{
			return (uint32_t)b[at] | ((uint32_t)b[at + 1] << 8) | ((uint32_t)b[at + 2] << 16) | ((uint32_t)b[at + 3] << 24);
		}

		uint32_t U16(std::vector<uint8_t> const &b, size_t at)
		{
			return (uint32_t)b[at] | ((uint32_t)b[at + 1] << 8);
		}

		bool Refuse(IntengExeRefusal *why, IntengExeRefusal what)
		{
			if (why)
			{
				*why = what;
			}

			return false;
		}

		IntengExeImage sharedImage;
		bool sharedHave = false;
		std::string sharedWhy = "inteng.exe was not looked for";
	}

	char const *IntengExe_Why(IntengExeRefusal why)
	{
		switch (why)
		{
		case IntengExeRefusal::None: return "read";
		case IntengExeRefusal::NotFound: return "inteng.exe is not next to this program nor in the IntengTools folder";
		case IntengExeRefusal::NotAFile: return "the inteng.exe found is not a plain file that could be read whole";
		case IntengExeRefusal::TooLong: return "the inteng.exe found is too big";
		case IntengExeRefusal::NotTheTracedExe: return "the inteng.exe found is not the one this editor was traced from (its sha256 differs)";
		case IntengExeRefusal::NotAnExe: return "the inteng.exe found is not a 32-bit Windows program";
		case IntengExeRefusal::BadSections: return "the inteng.exe found has a section table that does not fit the file";
		}

		return "refused";
	}

	std::string IntengExe_Sha256(uint8_t const *bytes, size_t size)
	{
		uint32_t state[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
		size_t done = 0;

		while (bytes && size - done >= 64)
		{
			Block(state, bytes + done);
			done += 64;
		}

		// the last bytes, the 0x80, the zeros and the length in bits (64 bits, the highest byte first)
		uint8_t tail[128] = {};
		size_t const left = size - done;

		if (left > 0)
		{
			memcpy(tail, bytes + done, left);
		}

		tail[left] = 0x80;

		size_t const tailBytes = left < 56 ? 64 : 128;
		uint64_t const bits = (uint64_t)size * 8u;

		for (int i = 0; i < 8; i++)
		{
			tail[tailBytes - 1 - i] = (uint8_t)(bits >> (8 * i));
		}

		Block(state, tail);

		if (tailBytes == 128)
		{
			Block(state, tail + 64);
		}

		static char const hex[] = "0123456789abcdef";
		std::string out;

		for (int i = 0; i < 8; i++)
		{
			for (int k = 3; k >= 0; k--)
			{
				uint8_t const byte = (uint8_t)(state[i] >> (8 * k));

				out += hex[byte >> 4];
				out += hex[byte & 15];
			}
		}

		return out;
	}

	bool IntengExe_Parse(std::vector<uint8_t> bytes, IntengExeImage *out, IntengExeRefusal *why)
	{
		if (!out)
		{
			return Refuse(why, IntengExeRefusal::NotAnExe);
		}

		*out = IntengExeImage();

		size_t const size = bytes.size();

		if (size < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z')
		{
			return Refuse(why, IntengExeRefusal::NotAnExe);
		}

		uint32_t const pe = U32(bytes, 0x3c);

		// "PE\0\0", the file header (20 bytes) and the PE32 optional header's fields up to the image base
		if (pe < 0x40 || (uint64_t)pe + 24 + 32 > size || memcmp(&bytes[pe], "PE\0\0", 4) != 0 || U16(bytes, pe + 4) != 0x14c)
		{
			return Refuse(why, IntengExeRefusal::NotAnExe);
		}

		uint32_t const sectionCount = U16(bytes, pe + 6);
		uint32_t const optionalBytes = U16(bytes, pe + 20);
		size_t const optional = pe + 24;

		if (optionalBytes < 96 || (uint64_t)optional + optionalBytes > size || U16(bytes, optional) != 0x10b)
		{
			return Refuse(why, IntengExeRefusal::NotAnExe);
		}

		uint32_t const imageBase = U32(bytes, optional + 28);
		uint64_t const table = (uint64_t)optional + optionalBytes;

		if (sectionCount == 0 || sectionCount > (uint32_t)kIntengExeMostSections || table + 40ull * sectionCount > size)
		{
			return Refuse(why, IntengExeRefusal::BadSections);
		}

		std::vector<IntengExeSection> sections;

		for (uint32_t i = 0; i < sectionCount; i++)
		{
			size_t const at = (size_t)table + 40 * i;
			uint32_t const virtualSize = U32(bytes, at + 8);
			uint32_t const rva = U32(bytes, at + 12);
			uint32_t rawSize = U32(bytes, at + 16);
			uint32_t const rawAt = U32(bytes, at + 20);

			// the file's bytes past the section's own size are padding, not the program's: only those inside both are read
			if (virtualSize != 0 && virtualSize < rawSize)
			{
				rawSize = virtualSize;
			}

			if ((uint64_t)rawAt + rawSize > size || (uint64_t)imageBase + rva + rawSize > 0xffffffffull)
			{
				return Refuse(why, IntengExeRefusal::BadSections);
			}

			if (rawSize == 0)
			{
				continue;   // nothing of it is in the file (all zeros at run time): never read
			}

			IntengExeSection one;

			one.va = imageBase + rva;
			one.rawSize = rawSize;
			one.rawAt = rawAt;
			sections.push_back(one);
		}

		if (sections.empty())
		{
			return Refuse(why, IntengExeRefusal::BadSections);
		}

		out->bytes.swap(bytes);
		out->imageBase = imageBase;
		out->sections.swap(sections);

		if (why)
		{
			*why = IntengExeRefusal::None;
		}

		return true;
	}

	bool IntengExe_Load(std::vector<std::string> const &paths, IntengExeImage *out, std::string *from, IntengExeRefusal *why)
	{
		for (std::string const &path : paths)
		{
			DWORD const what = GetFileAttributesA(path.c_str());

			if (what == INVALID_FILE_ATTRIBUTES)
			{
				continue;
			}

			if (from)
			{
				*from = path;
			}

			if (what & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))
			{
				return Refuse(why, IntengExeRefusal::NotAFile);
			}

			FILE *file = nullptr;

			if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
			{
				return Refuse(why, IntengExeRefusal::NotAFile);
			}

			std::vector<uint8_t> bytes(kIntengExeMostBytes + 1);
			size_t const got = fread(bytes.data(), 1, bytes.size(), file);
			bool const failed = ferror(file) != 0;

			fclose(file);

			if (failed)
			{
				return Refuse(why, IntengExeRefusal::NotAFile);
			}

			if (got > kIntengExeMostBytes)
			{
				return Refuse(why, IntengExeRefusal::TooLong);
			}

			bytes.resize(got);

			if (IntengExe_Sha256(bytes.data(), bytes.size()) != kIntengExeSha256)
			{
				return Refuse(why, IntengExeRefusal::NotTheTracedExe);
			}

			return IntengExe_Parse(std::move(bytes), out, why);
		}

		return Refuse(why, IntengExeRefusal::NotFound);
	}

	bool IntengExe_ReadBytes(IntengExeImage const &image, uint32_t va, size_t count, uint8_t *out)
	{
		if (!out || count == 0)
		{
			return false;
		}

		for (IntengExeSection const &s : image.sections)
		{
			if (va >= s.va && (uint64_t)va + count <= (uint64_t)s.va + s.rawSize && (uint64_t)s.rawAt + s.rawSize <= image.bytes.size())
			{
				memcpy(out, &image.bytes[s.rawAt + (va - s.va)], count);
				return true;
			}
		}

		return false;
	}

	bool IntengExe_ReadUint32(IntengExeImage const &image, uint32_t va, uint32_t *out)
	{
		uint8_t b[4];

		if (!out || !IntengExe_ReadBytes(image, va, 4, b))
		{
			return false;
		}

		*out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
		return true;
	}

	bool IntengExe_ReadInt32(IntengExeImage const &image, uint32_t va, int32_t *out)
	{
		uint32_t value = 0;

		if (!out || !IntengExe_ReadUint32(image, va, &value))
		{
			return false;
		}

		*out = (int32_t)value;
		return true;
	}

	void IntengExe_SetShared(IntengExeImage const *image, std::string const &why)
	{
		sharedHave = image != nullptr;
		sharedImage = image ? *image : IntengExeImage();
		sharedWhy = why;
	}

	IntengExeImage const *IntengExe_Shared(std::string *why)
	{
		if (why)
		{
			*why = sharedWhy;
		}

		return sharedHave ? &sharedImage : nullptr;
	}
}
