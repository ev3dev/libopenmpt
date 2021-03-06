/*
 * Load_mdl.cpp
 * ------------
 * Purpose: Digitrakker (MDL) module loader
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "Loaders.h"
#include "ChunkReader.h"

OPENMPT_NAMESPACE_BEGIN

#ifdef NEEDS_PRAGMA_PACK
#pragma pack(push, 1)
#endif

// MDL file header
struct PACKED MDLFileHeader
{
	char  id[4];	// "DMDL"
	uint8 version;
};

STATIC_ASSERT(sizeof(MDLFileHeader) == 5);


// RIFF-style Chunk
struct PACKED MDLChunk
{
	// 16-Bit chunk identifiers
	enum ChunkIdentifiers
	{
		idInfo			= MAGIC2LE('I','N'),
		idMessage		= MAGIC2LE('M','E'),
		idPats			= MAGIC2LE('P','A'),
		idPatNames		= MAGIC2LE('P','N'),
		idTracks		= MAGIC2LE('T','R'),
		idInstrs		= MAGIC2LE('I','I'),
		idVolEnvs		= MAGIC2LE('V','E'),
		idPanEnvs		= MAGIC2LE('P','E'),
		idFreqEnvs		= MAGIC2LE('F','E'),
		idSampleInfo	= MAGIC2LE('I','S'),
		ifSampleData	= MAGIC2LE('S','A'),
	};

	typedef ChunkIdentifiers id_type;

	uint16 id;
	uint32 length;

	size_t GetLength() const
	{
		return SwapBytesReturnLE(length);
	}

	id_type GetID() const
	{
		return static_cast<id_type>(SwapBytesReturnLE(id));
	}
};

STATIC_ASSERT(sizeof(MDLChunk) == 6);


struct PACKED MDLInfoBlock
{
	char   title[32];
	char   composer[20];
	uint16 numOrders;
	uint16 restartPos;
	uint8  globalVol;		// 1...255
	uint8  speed;			// 1...255
	uint8  tempo;			// 4...255
	uint8  chnSetup[32];

	// Convert all multi-byte numeric values to current platform's endianness or vice versa.
	void ConvertEndianness()
	{
		SwapBytesLE(numOrders);
		SwapBytesLE(restartPos);
	}
};

STATIC_ASSERT(sizeof(MDLInfoBlock) == 91);


// Sample header in II block
struct PACKED MDLSampleHeader
{
	uint8  smpNum;
	uint8  lastNote;
	uint8  volume;
	uint8  volEnvFlags; // 6 bits env #, 2 bits flags
	uint8  panning;
	uint8  panEnvFlags;
	uint16 fadeout;
	uint8  vibSpeed;
	uint8  vibDepth;
	uint8  vibSweep;
	uint8  vibType;
	uint8  reserved; // zero
	uint8  freqEnvFlags;

	// Convert all multi-byte numeric values to current platform's endianness or vice versa.
	void ConvertEndianness()
	{
		SwapBytesLE(fadeout);
	}
};

STATIC_ASSERT(sizeof(MDLSampleHeader) == 14);


// Part of the sample header that's common between v0 and v1.
struct PACKED MDLSampleInfoCommon
{
	uint8 sampleIndex;
	char  name[32];
	char  filename[8];
};

STATIC_ASSERT(sizeof(MDLSampleInfoCommon) == 41);


struct PACKED MDLSampleInfov0
{
	enum SampleFlags
	{
		smp16Bit		= 0x01,
		smpPingPong		= 0x02,

		smpNoPack		= 0x00,
		smpPack8Bit		= 0x04,
		smpPack16Bit	= 0x08,
		smpPackMask		= 0x0C,
	};

	uint16 c4speed;
	uint32 length;
	uint32 loopStart;
	uint32 loopLength;
	uint8  volume;
	uint8  flags;

	// Convert all multi-byte numeric values to current platform's endianness or vice versa.
	void ConvertEndianness()
	{
		SwapBytesLE(c4speed);
		SwapBytesLE(length);
		SwapBytesLE(loopStart);
		SwapBytesLE(loopLength);
	}

	// Convert an MDL sample header to OpenMPT's internal sample header.
	SampleIO ConvertToMPT(ModSample &mptSmp) const
	{
		mptSmp.nC5Speed = c4speed * 2;
		mptSmp.nLength = length;
		mptSmp.nVolume = volume;
		mptSmp.nLoopStart = loopStart;
		mptSmp.nLoopEnd = loopLength;
		if(loopLength != 0)
		{
			mptSmp.nLoopEnd += mptSmp.nLoopStart;
			mptSmp.uFlags.set(CHN_LOOP);
		}

		if(flags & smp16Bit)
		{
			mptSmp.uFlags.set(CHN_16BIT);
			mptSmp.nLength /= 2;
			mptSmp.nLoopStart /= 2;
			mptSmp.nLoopEnd /= 2;
		}

		if(flags & smpPingPong)
		{
			mptSmp.uFlags.set(CHN_PINGPONGLOOP);
		}

		return SampleIO(
			(flags & smp16Bit) ? SampleIO::_16bit : SampleIO::_8bit,
			SampleIO::mono,
			SampleIO::littleEndian,
			(flags & smpPackMask) ? SampleIO::MDL : SampleIO::signedPCM);
	}
};

STATIC_ASSERT(sizeof(MDLSampleInfov0) == 16);


struct PACKED MDLSampleInfo
{
	enum SampleFlags
	{
		smp16Bit		= 0x01,
		smpPingPong		= 0x02,

		smpNoPack		= 0x00,
		smpPack8Bit		= 0x04,
		smpPack16Bit	= 0x08,
		smpPackMask		= 0x0C,
	};

	uint32 c4speed;
	uint32 length;
	uint32 loopStart;
	uint32 loopLength;
	uint8  unused;		// was volume in v0.0, why it was changed I have no idea
	uint8  flags;

	// Convert all multi-byte numeric values to current platform's endianness or vice versa.
	void ConvertEndianness()
	{
		SwapBytesLE(c4speed);
		SwapBytesLE(length);
		SwapBytesLE(loopStart);
		SwapBytesLE(loopLength);
	}

	// Convert an MDL sample header to OpenMPT's internal sample header.
	SampleIO ConvertToMPT(ModSample &mptSmp) const
	{
		mptSmp.nC5Speed = c4speed * 2;
		mptSmp.nLength = length;
		mptSmp.nLoopStart = loopStart;
		mptSmp.nLoopEnd = loopLength;
		if(loopLength != 0)
		{
			mptSmp.nLoopEnd += mptSmp.nLoopStart;
			mptSmp.uFlags.set(CHN_LOOP);
		}

		if(flags & smp16Bit)
		{
			mptSmp.uFlags.set(CHN_16BIT);
			mptSmp.nLength /= 2;
			mptSmp.nLoopStart /= 2;
			mptSmp.nLoopEnd /= 2;
		}

		if(flags & smpPingPong)
		{
			mptSmp.uFlags.set(CHN_PINGPONGLOOP);
		}

		return SampleIO(
			(flags & smp16Bit) ? SampleIO::_16bit : SampleIO::_8bit,
			SampleIO::mono,
			SampleIO::littleEndian,
			(flags & smpPackMask) ? SampleIO::MDL : SampleIO::signedPCM);
	}
};

STATIC_ASSERT(sizeof(MDLSampleInfo) == 18);


struct PACKED MDLEnvelope
{
	uint8 envNum;
	struct
	{
		uint8 x;	// Delta value from last point, 0 means no more points defined
		uint8 y;	// 0...63
	} nodes[15];
	uint8 flags;
	uint8 loop;		// Lower 4 bits = start, upper 4 bits = end

	void ConvertToMPT(InstrumentEnvelope &mptEnv) const
	{
		mptEnv.dwFlags.reset();
		mptEnv.clear();
		mptEnv.reserve(15);
		int16 tick = -nodes[0].x;
		for(uint8 n = 0; n < 15; n++)
		{
			if(!nodes[n].x)
				break;
			tick += nodes[n].x;
			mptEnv.push_back(EnvelopeNode(tick, std::min(nodes[n].y, uint8(64)))); // actually 0-63
		}

		mptEnv.nLoopStart = (loop & 0x0F);
		mptEnv.nLoopEnd = (loop >> 4);
		mptEnv.nSustainStart = mptEnv.nSustainEnd = (flags & 0x0F);

		if(flags & 0x10) mptEnv.dwFlags.set(ENV_SUSTAIN);
		if(flags & 0x20) mptEnv.dwFlags.set(ENV_LOOP);
	}
};

STATIC_ASSERT(sizeof(MDLEnvelope) == 33);


struct PACKED MDLPatternHeader
{
	uint8 channels;
	uint8 lastRow;
	char  name[16];
};

STATIC_ASSERT(sizeof(MDLPatternHeader) == 18);


#ifdef NEEDS_PRAGMA_PACK
#pragma pack(pop)
#endif


enum
{
	MDLNOTE_NOTE	= 1 << 0,
	MDLNOTE_SAMPLE	= 1 << 1,
	MDLNOTE_VOLUME	= 1 << 2,
	MDLNOTE_EFFECTS	= 1 << 3,
	MDLNOTE_PARAM1	= 1 << 4,
	MDLNOTE_PARAM2	= 1 << 5,
};


static const uint8 MDLVibratoType[] = { VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_SINE };

static const ModCommand::COMMAND MDLEffTrans[] =
{
	/* 0 */ CMD_NONE,
	/* 1st column only */
	/* 1 */ CMD_PORTAMENTOUP,
	/* 2 */ CMD_PORTAMENTODOWN,
	/* 3 */ CMD_TONEPORTAMENTO,
	/* 4 */ CMD_VIBRATO,
	/* 5 */ CMD_ARPEGGIO,
	/* 6 */ CMD_NONE,
	/* Either column */
	/* 7 */ CMD_TEMPO,
	/* 8 */ CMD_PANNING8,
	/* 9 */ CMD_SETENVPOSITION,
	/* A */ CMD_NONE,
	/* B */ CMD_POSITIONJUMP,
	/* C */ CMD_GLOBALVOLUME,
	/* D */ CMD_PATTERNBREAK,
	/* E */ CMD_S3MCMDEX,
	/* F */ CMD_SPEED,
	/* 2nd column only */
	/* G */ CMD_VOLUMESLIDE, // up
	/* H */ CMD_VOLUMESLIDE, // down
	/* I */ CMD_RETRIG,
	/* J */ CMD_TREMOLO,
	/* K */ CMD_TREMOR,
	/* L */ CMD_NONE,
};


// receive an MDL effect, give back a 'normal' one.
static void ConvertMDLCommand(uint8_t &cmd, uint8_t &param)
//---------------------------------------------------------
{
	if(cmd >= CountOf(MDLEffTrans))
		return;

	uint8 origCmd = cmd;
	cmd = MDLEffTrans[cmd];

	switch(origCmd)
	{
#ifdef MODPLUG_TRACKER
	case 0x07: // Tempo
		// MDL supports any nonzero tempo value, but OpenMPT doesn't
		param = std::max(param, uint8(0x20));
		break;
#endif // MODPLUG_TRACKER
	case 0x08: // Panning
		param = (param & 0x7F) * 2u;
		break;
	case 0x0C:	// Global volume
		param = (param + 1) / 2u;
		break;
	case 0x0D: // Pattern Break
		// Convert from BCD
		param = 10 * (param >> 4) + (param & 0x0F);
		break;
	case 0x0E: // Special
		switch(param >> 4)
		{
		case 0x0: // unused
		case 0x3: // unused
		case 0x5: // Set Finetune
		case 0x8: // Set Samplestatus (loop type)
			cmd = CMD_NONE;
			break;
		case 0x1: // Pan Slide Left
			cmd = CMD_PANNINGSLIDE;
			param = (std::min<uint8>(param & 0x0F, 0x0E) << 4) | 0x0F;
			break;
		case 0x2: // Pan Slide Right
			cmd = CMD_PANNINGSLIDE;
			param = 0xF0 | std::min<uint8>(param & 0x0F, 0x0E);
			break;
		case 0x4: // Vibrato Waveform
			param = 0x30 | (param & 0x0F);
			break;
		case 0x6: // Pattern Loop
			param = 0xB0 | (param & 0x0F);
			break;
		case 0x7: // Tremolo Waveform
			param = 0x40 | (param & 0x0F);
			break;
		case 0x9: // Retrig
			cmd = CMD_RETRIG;
			param &= 0x0F;
			break;
		case 0xA: // Global vol slide up
			cmd = CMD_GLOBALVOLSLIDE;
			param = 0xF0 & (((param & 0x0F) + 1) << 3);
			break;
		case 0xB: // Global vol slide down
			cmd = CMD_GLOBALVOLSLIDE;
			param = ((param & 0x0F) + 1) >> 1;
			break;
		case 0xC: // Note cut
		case 0xD: // Note delay
		case 0xE: // Pattern delay
			// Nothing to change here
			break;
		case 0xF: // Offset -- further mangled later.
			cmd = CMD_OFFSET;
			break;
		}
		break;
	case 0x10: // Volslide up
		if(param < 0xE0)
		{
			// 00...DF regular slide - four times more precise than in XM
			param >>= 2;
			if(param > 0x0F)
				param = 0x0F;
			param <<= 4;
		} else if(param < 0xF0)
		{
			// E0...EF extra fine slide (on first tick, 4 times finer)
			param = (((param & 0x0F) << 2) | 0x0F);
		} else
		{
			// F0...FF regular fine slide (on first tick) - like in XM
			param = ((param << 4) | 0x0F);
		}
		break;
	case 0x11: // Volslide down
		if(param < 0xE0)
		{
			// 00...DF regular slide - four times more precise than in XM
			param >>= 2;
			if(param > 0x0F)
				param = 0x0F;
		} else if(param < 0xF0)
		{
			// E0...EF extra fine slide (on first tick, 4 times finer)
			param = (((param & 0x0F) >> 2) | 0xF0);
		} else
		{
			// F0...FF regular fine slide (on first tick) - like in XM
		}
		break;
	}
}


// Returns true if command was lost
static bool ImportMDLCommands(ModCommand &m, uint8 vol, uint8 e1, uint8 e2, uint8 p1, uint8 p2)
//---------------------------------------------------------------------------------------------
{
	// Map second effect values 1-6 to effects G-L
	if(e2 >= 1 && e2 <= 6)
		e2 += 15;

	ConvertMDLCommand(e1, p1);
	ConvertMDLCommand(e2, p2);
	/* From the Digitrakker documentation:
		* EFx -xx - Set Sample Offset
		This  is a  double-command.  It starts the
		sample at adress xxx*256.
		Example: C-5 01 -- EF1 -23 ->starts sample
		01 at address 12300 (in hex).
	Kind of screwy, but I guess it's better than the mess required to do it with IT (which effectively
	requires 3 rows in order to set the offset past 0xff00). If we had access to the entire track, we
	*might* be able to shove the high offset SAy into surrounding rows (or 2x MPTM #xx), but it wouldn't
	always be possible, it'd make the loader a lot uglier, and generally would be more trouble than
	it'd be worth to implement.

	What's more is, if there's another effect in the second column, it's ALSO processed in addition to the
	offset, and the second data byte is shared between the two effects. */
	if(e1 == CMD_OFFSET)
	{
		// EFy -xx => offset yxx00
		p1 = (p1 & 0x0F) ? 0xFF : p2;
		if(e2 == CMD_OFFSET)
			e2 = CMD_NONE;
	} else if (e2 == CMD_OFFSET)
	{
		// --- EFy => offset y0000 (best we can do without doing a ton of extra work is 0xff00)
		p2 = (p2 & 0x0F) ? 0xFF : 0;
	}

	if(vol)
	{
		m.volcmd = VOLCMD_VOLUME;
		m.vol = (vol + 2) / 4u;
	}

	// If we have Dxx + G00, or Dxx + H00, combine them into Lxx/Kxx.
	ModCommand::CombineEffects(e1, p1, e2, p2);

	bool lostCommand = false;
	// Try to fit the "best" effect into e2.
	if(e1 == CMD_NONE)
	{
		// Easy
	} else if(e2 == CMD_NONE)
	{
		// Almost as easy
		e2 = e1;
		p2 = p1;
		e1 = CMD_NONE;
	} else if(e1 == e2 && e1 != CMD_S3MCMDEX)
	{
		// Digitrakker processes the effects left-to-right, so if both effects are the same, the
		// second essentially overrides the first.
		e1 = CMD_NONE;
	} else if(!vol)
	{
		lostCommand |= !ModCommand::TwoRegularCommandsToMPT(e1, p1, e2, p2);
		m.volcmd = e1;
		m.vol = p1;
	} else
	{
		if(ModCommand::GetEffectWeight((ModCommand::COMMAND)e1) > ModCommand::GetEffectWeight((ModCommand::COMMAND)e2))
		{
			std::swap(e1, e2);
			std::swap(p1, p2);
		}
	}

	m.command = e2;
	m.param = p2;
	return lostCommand;
}


static void MDLReadEnvelopes(FileReader file, std::vector<MDLEnvelope> &envelopes)
//--------------------------------------------------------------------------------
{
	if(!file.CanRead(1))
		return;

	envelopes.resize(64);
	uint8 numEnvs = file.ReadUint8();
	while(numEnvs--)
	{
		MDLEnvelope mdlEnv;
		if(!file.ReadStruct(mdlEnv) || mdlEnv.envNum > 63)
			continue;
		envelopes[mdlEnv.envNum] = mdlEnv;
	}
}


static void CopyEnvelope(InstrumentEnvelope &mptEnv, uint8 flags, std::vector<MDLEnvelope> &envelopes)
//----------------------------------------------------------------------------------------------------
{
	uint8 envNum = flags & 0x3F;
	if(envNum < envelopes.size())
		envelopes[envNum].ConvertToMPT(mptEnv);
	mptEnv.dwFlags.set(ENV_ENABLED, (flags & 0x80) && !mptEnv.empty());
}


bool CSoundFile::ReadMDL(FileReader &file, ModLoadingFlags loadFlags)
//-------------------------------------------------------------------
{
	file.Rewind();
	MDLFileHeader fileHeader;
	if(!file.ReadStruct(fileHeader)
		|| memcmp(fileHeader.id, "DMDL", 4)
		|| fileHeader.version >= 0x20)
	{
		return false;
	} else if(loadFlags == onlyVerifyHeader)
	{
		return true;
	}

	ChunkReader chunkFile(file);
	ChunkReader::ChunkList<MDLChunk> chunks = chunkFile.ReadChunks<MDLChunk>(0);

	// Read global info
	FileReader chunk = chunks.GetChunk(MDLChunk::idInfo);
	MDLInfoBlock info;
	if(!chunk.IsValid() || !chunk.ReadConvertEndianness(info))
	{
		return false;
	}

	InitializeGlobals(MOD_TYPE_MDL);
	m_SongFlags = SONG_ITCOMPATGXX;
	m_playBehaviour.set(kPerChannelGlobalVolSlide);
	m_playBehaviour.reset(kITVibratoTremoloPanbrello);
	m_playBehaviour.reset(kITSCxStopsSample);	// Gate effect in underbeat.mdl

	m_madeWithTracker = std::string("Digitrakker ") + (
		(fileHeader.version == 0x11) ? "3" // really could be 2.99b - close enough
		: (fileHeader.version == 0x10) ? "2.3"
		: (fileHeader.version == 0x00) ? "2.0 - 2.2b" // there was no 1.x release
		: "");

	mpt::String::Read<mpt::String::spacePadded>(m_songName, info.title);
	{
		std::string artist;
		mpt::String::Read<mpt::String::spacePadded>(artist, info.composer);
		m_songArtist = mpt::ToUnicode(mpt::CharsetCP437, artist);
	}

	m_nDefaultGlobalVolume = info.globalVol + 1;
	m_nDefaultSpeed = Clamp(info.speed, uint8(1), uint8(255));
	m_nDefaultTempo.Set(Clamp(info.tempo, uint8(4), uint8(255)));

	Order.ReadAsByte(chunk, info.numOrders);
	Order.SetRestartPos(info.restartPos);

	m_nChannels = 0;
	for(CHANNELINDEX c = 0; c < 32; c++)
	{
		ChnSettings[c].Reset();
		ChnSettings[c].nPan = (info.chnSetup[c] & 0x7F) * 2u;
		if(ChnSettings[c].nPan == 254)
			ChnSettings[c].nPan = 256;
		if(info.chnSetup[c] & 0x80)
			ChnSettings[c].dwFlags.set(CHN_MUTE);
		else
			m_nChannels = c + 1;
		chunk.ReadString<mpt::String::spacePadded>(ChnSettings[c].szName, 8);
	}

	// Read song message
	chunk = chunks.GetChunk(MDLChunk::idMessage);
	m_songMessage.Read(chunk, chunk.GetLength(), SongMessage::leCR);

	// Read sample info and data
	chunk = chunks.GetChunk(MDLChunk::idSampleInfo);
	if(chunk.IsValid())
	{
		FileReader dataChunk = chunks.GetChunk(MDLChunk::ifSampleData);

		uint8 numSamples = chunk.ReadUint8();
		for(uint8 smp = 0; smp < numSamples; smp++)
		{
			MDLSampleInfoCommon header;
			if(!chunk.ReadStruct(header) || header.sampleIndex == 0)
				continue;
			#if 1
				STATIC_ASSERT(MPT_MAX_UNSIGNED_VALUE(header.sampleIndex) < MAX_SAMPLES);
			#else
				MPT_MAYBE_CONSTANT_IF(header.sampleIndex >= MAX_SAMPLES)
					continue;
			#endif

			if(header.sampleIndex > GetNumSamples())
				m_nSamples = header.sampleIndex;

			ModSample &sample = Samples[header.sampleIndex];
			sample.Initialize();

			mpt::String::Read<mpt::String::spacePadded>(m_szNames[header.sampleIndex], header.name);
			mpt::String::Read<mpt::String::spacePadded>(sample.filename, header.filename);

			SampleIO sampleIO;
			if(fileHeader.version >= 0x10)
			{
				MDLSampleInfo sampleHeader;
				chunk.ReadConvertEndianness(sampleHeader);
				sampleIO = sampleHeader.ConvertToMPT(sample);
			} else
			{
				MDLSampleInfov0 sampleHeader;
				chunk.ReadConvertEndianness(sampleHeader);
				sampleIO = sampleHeader.ConvertToMPT(sample);
			}

			if((loadFlags & loadSampleData) && (sample.nLength || sampleIO.GetEncoding() == SampleIO::MDL))
			{
				sampleIO.ReadSample(sample, dataChunk);
			}
		}
	}

	chunk = chunks.GetChunk(MDLChunk::idInstrs);
	if(chunk.IsValid())
	{
		std::vector<MDLEnvelope> volEnvs, panEnvs, pitchEnvs;
		MDLReadEnvelopes(chunks.GetChunk(MDLChunk::idVolEnvs), volEnvs);
		MDLReadEnvelopes(chunks.GetChunk(MDLChunk::idPanEnvs), panEnvs);
		MDLReadEnvelopes(chunks.GetChunk(MDLChunk::idFreqEnvs), pitchEnvs);

		uint8 numInstruments = chunk.ReadUint8();
		for(uint8 i = 0; i < numInstruments; i++)
		{
			uint8 ins = chunk.ReadUint8();
			uint8 numSamples = chunk.ReadUint8();
			uint8 firstNote = 0;
			ModInstrument *mptIns = nullptr;
			if(ins == 0
				|| !chunk.CanRead(32 + sizeof(MDLSampleHeader) * numSamples)
				|| (mptIns = AllocateInstrument(ins)) == nullptr)
			{
				chunk.Skip(32 + sizeof(MDLSampleHeader) * numSamples);
				continue;
			}
			m_nInstruments = std::max<INSTRUMENTINDEX>(m_nInstruments, ins);

			chunk.ReadString<mpt::String::spacePadded>(mptIns->name, 32);
			while(numSamples--)
			{
				MDLSampleHeader sampleHeader;
				chunk.ReadConvertEndianness(sampleHeader);
				if(sampleHeader.smpNum == 0)
					continue;
				#if 1
					STATIC_ASSERT(MPT_MAX_UNSIGNED_VALUE(sampleHeader.smpNum) < MAX_SAMPLES);
				#else
					MPT_MAYBE_CONSTANT_IF(sampleHeader.smpNum >= MAX_SAMPLES)
						continue;
				#endif

				LimitMax(sampleHeader.lastNote, static_cast<uint8>(CountOf(mptIns->Keyboard)));
				for(uint8 n = firstNote; n <= sampleHeader.lastNote; n++)
				{
					mptIns->Keyboard[n] = sampleHeader.smpNum;
				}
				firstNote = sampleHeader.lastNote + 1;

				CopyEnvelope(mptIns->VolEnv, sampleHeader.volEnvFlags, volEnvs);
				CopyEnvelope(mptIns->PanEnv, sampleHeader.panEnvFlags, panEnvs);
				CopyEnvelope(mptIns->PitchEnv, sampleHeader.freqEnvFlags, pitchEnvs);
				mptIns->nFadeOut = (sampleHeader.fadeout + 1u) / 2u;
#ifdef MODPLUG_TRACKER
				if((mptIns->VolEnv.dwFlags & (ENV_ENABLED | ENV_LOOP)) == ENV_ENABLED)
				{
					// Fade-out is only supposed to happen on key-off, not at the end of a volume envelope.
					// Fake it by putting a loop at the end.
					mptIns->VolEnv.nLoopStart = mptIns->VolEnv.nLoopEnd = static_cast<uint8>(mptIns->VolEnv.size() - 1);
					mptIns->VolEnv.dwFlags.set(ENV_LOOP);
				}
				for(InstrumentEnvelope::iterator it = mptIns->PitchEnv.begin(); it != mptIns->PitchEnv.end(); it++)
				{
					// Scale pitch envelope
					it->value = (it->value * 6u) / 16u;
				}
#endif // MODPLUG_TRACKER

				// Samples were already initialized above. Let's hope they are not going to be re-used with different volume / panning / vibrato...
				ModSample &mptSmp = Samples[sampleHeader.smpNum];

				// Not quite correct - this flag literally enables and disables the default volume of a sample. If you disable this flag,
				// the sample volume of a previously sample is re-used, even if you put an instrument number next to the note.
				if(sampleHeader.volEnvFlags & 0x40)
					mptSmp.nVolume = sampleHeader.volume;
				mptSmp.nPan = std::min<uint16>(sampleHeader.panning * 2, 254);
				mptSmp.nVibType = MDLVibratoType[sampleHeader.vibType & 3];
				mptSmp.nVibSweep = sampleHeader.vibSweep;
				mptSmp.nVibDepth = sampleHeader.vibDepth;
				mptSmp.nVibRate = sampleHeader.vibSpeed;
				if(sampleHeader.panEnvFlags & 0x40)
					mptSmp.uFlags.set(CHN_PANNING);
			}
		}
	}

	// Read pattern tracks
	std::vector<FileReader> tracks;
	if((loadFlags & loadPatternData) && (chunk = chunks.GetChunk(MDLChunk::idTracks)).IsValid())
	{
		uint32 numTracks = chunk.ReadUint16LE();
		tracks.resize(numTracks + 1);
		for(uint32 i = 1; i <= numTracks; i++)
		{
			tracks[i] = chunk.ReadChunk(chunk.ReadUint16LE());
		}
	}

	// Read actual patterns
	if((loadFlags & loadPatternData) && (chunk = chunks.GetChunk(MDLChunk::idPats)).IsValid())
	{
		PATTERNINDEX numPats = chunk.ReadUint8();

		// In case any muted channels contain data, be sure that we import them as well.
		for(PATTERNINDEX pat = 0; pat < numPats; pat++)
		{
			CHANNELINDEX numChans = 32;
			if(fileHeader.version >= 0x10)
			{
				MDLPatternHeader patHead;
				chunk.ReadStruct(patHead);
				if(patHead.channels > m_nChannels && patHead.channels <= 32)
					m_nChannels = patHead.channels;
				numChans = patHead.channels;
			}
			for(CHANNELINDEX chn = 0; chn < numChans; chn++)
			{
				if(chunk.ReadUint16LE() > 0 && chn >= m_nChannels)
					m_nChannels = chn + 1;
			}
		}
		chunk.Seek(1);

		for(PATTERNINDEX pat = 0; pat < numPats; pat++)
		{
			CHANNELINDEX numChans = 32;
			ROWINDEX numRows = 64;
			char name[17] = "";
			if(fileHeader.version >= 0x10)
			{
				MDLPatternHeader patHead;
				chunk.ReadStruct(patHead);
				numChans = patHead.channels;
				numRows = patHead.lastRow + 1;
				mpt::String::Read<mpt::String::spacePadded>(name, patHead.name);
			}

			if(!Patterns.Insert(pat, numRows))
			{
				chunk.Skip(2 * numChans);
				continue;
			}
			Patterns[pat].SetName(name);

			for(CHANNELINDEX chn = 0; chn < numChans; chn++)
			{
				uint16 trkNum = chunk.ReadUint16LE();
				if(!trkNum || trkNum >= tracks.size() || chn >= m_nChannels)
					continue;

				FileReader &track = tracks[trkNum];
				track.Rewind();
				ROWINDEX row = 0;
				while(row < numRows && track.CanRead(1))
				{
					ModCommand *m = Patterns[pat].GetpModCommand(row, chn);
					uint8 b = track.ReadUint8();
					uint8 x = (b >> 2), y = (b & 3);
					switch(y)
					{
					case 0:
						// (x + 1) empty notes follow
						row += x + 1;
						break;
					case 1:
						// Repeat previous note (x + 1) times
						if(row > 0)
						{
							ModCommand &orig = *Patterns[pat].GetpModCommand(row - 1, chn);
							do
							{
								*m = orig;
								m += m_nChannels;
								row++;
							} while (row < numRows && x--);
						}
						break;
					case 2:
						// Copy note from row x
						if(row > x)
						{
							*m = *Patterns[pat].GetpModCommand(x, chn);
						}
						row++;
						break;
					case 3:
						// New note data
						if(x & MDLNOTE_NOTE)
						{
							b = track.ReadUint8();
							m->note = (b > 120) ? NOTE_KEYOFF : b;
						}
						if(x & MDLNOTE_SAMPLE)
						{
							m->instr = track.ReadUint8();
						}
						{
							uint8 vol = 0, e1 = 0, e2 = 0, p1 = 0, p2 = 0;
							if(x & MDLNOTE_VOLUME)
							{
								vol = track.ReadUint8();
							}
							if(x & MDLNOTE_EFFECTS)
							{
								b = track.ReadUint8();
								e1 = (b & 0x0F);
								e2 = (b >> 4);
							}
							if(x & MDLNOTE_PARAM1)
								p1 = track.ReadUint8();
							if(x & MDLNOTE_PARAM2)
								p2 = track.ReadUint8();
							ImportMDLCommands(*m, vol, e1, e2, p1, p2);
						}

						row++;
						break;
					}
				}
			}
		}
	}

	if((loadFlags & loadPatternData) && (chunk = chunks.GetChunk(MDLChunk::idPatNames)).IsValid())
	{
		PATTERNINDEX i = 0;
		while(i < Patterns.Size() && chunk.CanRead(16))
		{
			char name[17];
			chunk.ReadString<mpt::String::spacePadded>(name, 16);
			Patterns[i].SetName(name);
		}
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////
// MDL Sample Unpacking

// MDL Huffman ReadBits compression
uint16 MDLReadBits(uint32 &bitbuf, uint32 &bitnum, const uint8 *(&ibuf), size_t &bytesLeft, int8 n)
//-------------------------------------------------------------------------------------------------
{
	uint16 v = (uint16)(bitbuf & ((1 << n) - 1) );
	bitbuf >>= n;
	bitnum -= n;
	if (bitnum <= 24)
	{
		if(!bytesLeft)
		{
			bitnum += 8;
			return uint16_max;
		}
		bitbuf |= (((uint32)(*ibuf++)) << bitnum);
		bitnum += 8;
		bytesLeft--;
	}
	return v;
}


OPENMPT_NAMESPACE_END
