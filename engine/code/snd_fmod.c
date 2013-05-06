/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// snd_fmod.c: FMOD sound system implementation for music playback

// Developed by Jacques Krige
// Ultimate Quake Engine
// http://www.corvinstein.com


#include "quakedef.h"


#ifdef _WIN32
#include "winquake.h"
#endif


#ifndef UQE_FMOD_CDAUDIO
#include "cdaudio.h"
#endif


#include "../fmod-4/inc/fmod.h"
#include "../fmod-4/inc/fmod_errors.h"


typedef struct
{
	void			*data;
	int				length;
	char			filename[MAX_QPATH];
} SND_File_t;

typedef struct
{
	float			volume;
	FMOD_CHANNEL	*channel;
	int				track;
	qboolean		inuse;
	qboolean		locked;
	qboolean		looping;
	qboolean		paused;
} SND_Channel_t;



SND_File_t		SND_File;
SND_Channel_t	SND_MusicChannel;

FMOD_SYSTEM		*fmod_system;

FMOD_SOUND		*fmod_music;
FMOD_SOUND		*fmod_musicCD;
FMOD_SOUND		*fmod_musicCD_subsound;


FMOD_RESULT		result;

int				SND_Bits;
int				SND_Rate;
qboolean		SND_Initialised;
int				SND_HardwareChannels;
int				SND_SoftwareChannels;

int				numtracks;
qboolean		SND_InitialisedCD;

char			oldbgmtype[16];


// forward declarations
void FMOD_Restart (void);
void FMOD_MusicStart (char *name, byte track, qboolean loop, qboolean notify, qboolean resume);
void FMOD_MusicStartConsole (void);




// ===================================================================================
//
//  CUSTOM FMOD FILE ROUTINES TO ALLOW PAK/PK3 FILE ACCESS
//
// ===================================================================================

qboolean SND_FOpen (const char *name, qboolean midi, qboolean resume)
{
	int len;
	char file[MAX_QPATH];
	char filefull[MAX_QPATH];
	FILE	*f;


	if(resume == true)
	{
		strcpy(file, name);
	}
	else
	{
		if (midi == true)
			sprintf(file, "midi/%s", name);
		else
			sprintf(file, "mod/%s", name);
	}


	if(midi == true)
	{
		sprintf(filefull, "%s.mid", file);
		len = COM_FOpenFile ((char *)filefull, &f);
	}
	else
	{
		sprintf(filefull, "%s.ogg", file);
		len = COM_FOpenFile ((char *)filefull, &f);
		if(len < 1)
		{
			sprintf(filefull, "%s.mp3", file);
			len = COM_FOpenFile ((char *)filefull, &f);
		}
		if(len < 1)
		{
			sprintf(filefull, "%s.wav", file);
			len = COM_FOpenFile ((char *)filefull, &f);
		}
	}


	if(len < 1)
	{
		Con_Printf("SND_FOpen: Failed to open %s, file not found\n", filefull);
		return false;
	}

	if(!SND_File.length)
	{
		strcpy(SND_File.filename, filefull);
		SND_File.length = len;
		SND_File.data = COM_FReadFile(f, len);

		Con_DPrintf("SND_FOpen: Sucessfully opened %s\n", filefull);
		return true;
	}

	Con_SafePrintf("SND_FOpen: Failed to open %s, insufficient handles\n", filefull);
	return false;
}

void SND_FClose (void)
{
	if (!SND_File.data)
		return;

	SND_File.length = 0;
	strcpy(SND_File.filename, "\0");

	if (SND_File.data)
		free(SND_File.data);

	SND_File.data = NULL;
}




// ===================================================================================
//
//  STARTUP AND SHUTDOWN ROUTINES
//
// ===================================================================================

void FMOD_ERROR(FMOD_RESULT result, qboolean notify, qboolean syserror)
{
	if (result != FMOD_OK)
	{
		if (syserror == false)
		{
			if (notify == true)
				Con_Printf("%s\n", FMOD_ErrorString(result));
		}
		else
			Sys_Error("FMOD: %s\n", FMOD_ErrorString(result));
	}
}




void CDA_Startup (qboolean notify)
{
#ifdef UQE_FMOD_CDAUDIO

	int i;
	int numdrives;

	if (fmod_musicCD)
		return;

	// bump up the file buffer size a bit from the 16k default for CDDA, because it is a slower medium.
	result = FMOD_System_SetStreamBufferSize(fmod_system, 64*1024, FMOD_TIMEUNIT_RAWBYTES);
    FMOD_ERROR(result, notify, false);

	result = FMOD_System_GetNumCDROMDrives(fmod_system, &numdrives);
	FMOD_ERROR(result, notify, false);

	for(i = 0; i < numdrives; i++)
	{
		char drivename[MAX_QPATH];
		char scsiname[MAX_QPATH];
		char devicename[MAX_QPATH];

		result = FMOD_System_GetCDROMDriveName(fmod_system, i, drivename, MAX_QPATH, scsiname, MAX_QPATH, devicename, MAX_QPATH);
		FMOD_ERROR(result, notify, false);

		if(result == FMOD_OK)
		{
			result = FMOD_System_CreateStream(fmod_system, drivename, FMOD_OPENONLY, 0, &fmod_musicCD);

			if(result == FMOD_OK)
			{
				result = FMOD_Sound_GetNumSubSounds(fmod_musicCD, &numtracks);
				FMOD_ERROR(result, notify, false);

				if(result == FMOD_OK)
				{
					Con_Printf("CD Audio Initialized (%s)\n", drivename);
					SND_InitialisedCD = true;

					break;
				}
			}
		}
	}
	FMOD_ERROR(result, notify, false);

#else
	CDAudio_Init();
#endif
}

void CDA_Shutdown (void)
{
#ifdef UQE_FMOD_CDAUDIO

	//if(SND_InitialisedCD == false)
		//return;

	if (fmod_musicCD_subsound)
		FMOD_Sound_Release(fmod_musicCD_subsound);

	if (fmod_musicCD)
		FMOD_Sound_Release(fmod_musicCD);

#else
	CDAudio_Shutdown ();
#endif
}




void FMOD_Startup (void)
{
	FMOD_CAPS			caps;
	FMOD_SPEAKERMODE	speakermode;
	FMOD_OUTPUTTYPE		fmod_output;
	unsigned int		version;
	int numdrivers;
	char name[256];

	result = FMOD_System_Create(&fmod_system);
    FMOD_ERROR(result, true, false);

    result = FMOD_System_GetVersion(fmod_system, &version);
    FMOD_ERROR(result, true, false);

    if (version < FMOD_VERSION)
    {
		Con_Printf("\nFMOD version incorrect, found v%1.2f, requires v%1.2f or newer\n", version, FMOD_VERSION);
        return;
    }

	result = FMOD_System_GetNumDrivers(fmod_system, &numdrivers);
	FMOD_ERROR(result, true, false);

	if (numdrivers == 0)
	{
		result = FMOD_System_SetOutput(fmod_system, FMOD_OUTPUTTYPE_NOSOUND);
		FMOD_ERROR(result, true, false);
	}
	else
	{
		result = FMOD_System_SetOutput(fmod_system, FMOD_OUTPUTTYPE_AUTODETECT);
		FMOD_ERROR(result, true, false);

		result = FMOD_System_GetDriverCaps(fmod_system, 0, &caps, NULL, &speakermode);
		FMOD_ERROR(result, true, false);

		// set the user selected speaker mode
		result = FMOD_System_SetSpeakerMode(fmod_system, FMOD_SPEAKERMODE_STEREO /*speakermode*/);
		FMOD_ERROR(result, true, false);

		if (caps & FMOD_CAPS_HARDWARE_EMULATED)
		{
			// the user has the 'Acceleration' slider set to off. this is really bad for latency!
			result = FMOD_System_SetDSPBufferSize(fmod_system, 1024, 10);
			FMOD_ERROR(result, true, false);

			Con_Printf("\nHardware Acceleration is turned off!\n");
		}

		result = FMOD_System_GetDriverInfo(fmod_system, 0, name, 256, NULL);
		FMOD_ERROR(result, true, false);
		
		if (strstr(name, "SigmaTel"))
		{
			// Sigmatel sound devices crackle for some reason if the format is PCM 16bit.
			// PCM floating point output seems to solve it.
			result = FMOD_System_SetSoftwareFormat(fmod_system, 48000, FMOD_SOUND_FORMAT_PCMFLOAT, 0, 0, FMOD_DSP_RESAMPLER_LINEAR);
			FMOD_ERROR(result, true, false);
		}
	}

	result = FMOD_System_GetSoftwareChannels(fmod_system, &SND_SoftwareChannels);
	FMOD_ERROR(result, true, false);

	result = FMOD_System_GetSoftwareFormat(fmod_system, &SND_Rate, NULL, NULL, NULL, NULL, &SND_Bits);
	FMOD_ERROR(result, true, false);

	result = FMOD_System_Init(fmod_system, MAX_CHANNELS, FMOD_INIT_NORMAL, NULL);
    FMOD_ERROR(result, true, false);

	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		// the speaker mode selected isn't supported by this soundcard. Switch it back to stereo...
		result = FMOD_System_SetSpeakerMode(fmod_system, FMOD_SPEAKERMODE_STEREO);
		FMOD_ERROR(result, true, false);

		// ... and re-init.
		result = FMOD_System_Init(fmod_system, MAX_CHANNELS, FMOD_INIT_NORMAL, NULL);
		FMOD_ERROR(result, true, false);
	}

	result = FMOD_System_GetSpeakerMode(fmod_system, &speakermode);
	FMOD_ERROR(result, true, false);

	result = FMOD_System_GetOutput(fmod_system, &fmod_output);
	FMOD_ERROR(result, true, false);

	result = FMOD_System_GetHardwareChannels(fmod_system, &SND_HardwareChannels);
	FMOD_ERROR(result, true, false);


	// print all the sound information to the console

	Con_Printf("\nFMOD version %01x.%02x.%02x\n", (FMOD_VERSION >> 16) & 0xff, (FMOD_VERSION >> 8) & 0xff, FMOD_VERSION & 0xff);

	switch(fmod_output)
	{
		case FMOD_OUTPUTTYPE_NOSOUND:
			Con_Printf("using No Sound\n");
			break;

		case FMOD_OUTPUTTYPE_DSOUND:
			Con_Printf("using Microsoft DirectSound\n");
			break;

		case FMOD_OUTPUTTYPE_WINMM:
			Con_Printf("using Windows Multimedia\n");
			break;

		case FMOD_OUTPUTTYPE_WASAPI:
			Con_Printf("using Windows Audio Session API\n");
			break;

		case FMOD_OUTPUTTYPE_ASIO:
			Con_Printf("using Low latency ASIO\n");
			break;
	}
	
	Con_Printf("   software channels: %i\n", SND_SoftwareChannels);
	Con_Printf("   hardware channels: %i\n", SND_HardwareChannels);
	Con_Printf("   %i bits/sample\n", SND_Bits);
	Con_Printf("   %i bytes/sec\n", SND_Rate);

	switch(speakermode)
	{
		case FMOD_SPEAKERMODE_RAW:
			Con_Printf("Speaker Output: Raw\n");
			break;

		case FMOD_SPEAKERMODE_MONO:
			Con_Printf("Speaker Output: Mono\n");
			break;

		case FMOD_SPEAKERMODE_STEREO:
			Con_Printf("Speaker Output: Stereo\n");
			break;

		case FMOD_SPEAKERMODE_QUAD:
			Con_Printf("Speaker Output: Quad\n");
			break;

		case FMOD_SPEAKERMODE_SURROUND:
			Con_Printf("Speaker Output: Surround\n");
			break;

		case FMOD_SPEAKERMODE_5POINT1:
			Con_Printf("Speaker Output: 5.1\n");
			break;

		case FMOD_SPEAKERMODE_7POINT1:
			Con_Printf("Speaker Output: 7.1\n");
			break;

		case FMOD_SPEAKERMODE_SRS5_1_MATRIX:
			Con_Printf("Speaker Output: Stereo compatible\n");
			break;

		case FMOD_SPEAKERMODE_MYEARS:
			Con_Printf("Speaker Output: Headphones\n");
			break;

		default:
			Con_Printf("Speaker Output: Unknown\n");
	}

	if(SND_File.data)
		free(SND_File.data);
	SND_File.data = NULL;
	SND_File.length = 0;
	strcpy(SND_File.filename, "\0");


	CDA_Startup(true);


	SND_Initialised = true;
}

void FMOD_Init (void)
{
	SND_Initialised = false;
	SND_InitialisedCD = false;

	if (COM_CheckParm("-nosound"))
		return;

	Cmd_AddCommand("fmod_restart", FMOD_Restart);
	Cmd_AddCommand("fmod_playmusic", FMOD_MusicStartConsole);
	Cmd_AddCommand("fmod_stopmusic", FMOD_MusicStop);
	Cmd_AddCommand("fmod_pausemusic", FMOD_MusicPause);
	Cmd_AddCommand("fmod_resumemusic", FMOD_MusicResume);

	FMOD_Startup();

	// Lock Music channel
	SND_MusicChannel.volume = 0.0f;
	SND_MusicChannel.channel = NULL;
	SND_MusicChannel.track = 0;
	SND_MusicChannel.inuse = false;
	SND_MusicChannel.locked = true;
	SND_MusicChannel.looping = false;
	SND_MusicChannel.paused = false;

	Con_Printf("------------------------------------\n");
}

void FMOD_Shutdown (void)
{
	if (COM_CheckParm("-nosound"))
	{
		SND_Initialised = false;
		SND_InitialisedCD = false;
		return;
	}

	if (SND_MusicChannel.channel)
		FMOD_Channel_Stop(SND_MusicChannel.channel);

	CDA_Shutdown();


	if (fmod_music)
		FMOD_Sound_Release(fmod_music);

	if (fmod_system)
	{
		result = FMOD_System_Close(fmod_system);
		FMOD_ERROR(result, true, false);
		result = FMOD_System_Release(fmod_system);
		FMOD_ERROR(result, true, false);
	}

	SND_Initialised = false;
	SND_InitialisedCD = false;
}

void FMOD_Restart (void)
{
	char filename[MAX_QPATH];

	if (COM_CheckParm("-nosound"))
	{
		SND_Initialised = false;
		SND_InitialisedCD = false;
		return;
	}

	// save music info
	if(SND_MusicChannel.inuse == true)
	{
		strcpy(filename, SND_File.filename);
		FMOD_MusicStop();
	}

	FMOD_Shutdown();
	FMOD_Startup();

	// restart music if needed
	FMOD_MusicStart(filename, 0, SND_MusicChannel.looping, false, true);
}




// ===================================================================================
//
//  CD AUDIO CONTROL ROUTINES
//
// ===================================================================================

void CDA_Start (int track, qboolean loop, qboolean notify)
{
#ifdef UQE_FMOD_CDAUDIO

	if (SND_InitialisedCD == false)
		return;

	if(SND_MusicChannel.inuse == true)
		FMOD_MusicStop();

	if(loop == true)
	{
		SND_MusicChannel.looping = true;
		result = FMOD_Sound_SetMode(fmod_musicCD, FMOD_LOOP_NORMAL);
		FMOD_ERROR(result, true, false);
	}
	else
	{
		SND_MusicChannel.looping = false;
		result = FMOD_Sound_SetMode(fmod_musicCD, FMOD_LOOP_OFF);
		FMOD_ERROR(result, true, false);
	}

	// fmod track numbers starts at zero
	result = FMOD_Sound_GetSubSound(fmod_musicCD, track-1, &fmod_musicCD_subsound);
	FMOD_ERROR(result, notify, false);

	result = FMOD_System_GetChannel(fmod_system, 0, &SND_MusicChannel.channel);
	FMOD_ERROR(result, true, false);

	result = FMOD_System_PlaySound(fmod_system, FMOD_CHANNEL_REUSE, fmod_musicCD_subsound, FALSE, &SND_MusicChannel.channel);
    FMOD_ERROR(result, notify, false);


	SND_MusicChannel.inuse = true;

#else
	CDAudio_Play((byte)track, loop);
#endif
}

void CDA_Stop (void)
{
#ifdef UQE_FMOD_CDAUDIO

	if(SND_InitialisedCD == false || SND_MusicChannel.inuse == false)
		return;

	if(SND_MusicChannel.channel)
	{
		result = FMOD_Channel_Stop(SND_MusicChannel.channel);
		FMOD_ERROR(result, true, false);
	}

	if(fmod_musicCD_subsound)
	{
		result = FMOD_Sound_Release(fmod_musicCD_subsound);
		FMOD_ERROR(result, true, false);
	}

	if(fmod_musicCD)
	{
		result = FMOD_Sound_Release(fmod_musicCD);
		FMOD_ERROR(result, true, false);
	}

	SND_MusicChannel.inuse = false;

#else
	CDAudio_Stop();
#endif
}

void CDA_Pause (void)
{
#ifdef UQE_FMOD_CDAUDIO

	if(SND_InitialisedCD == false || SND_MusicChannel.inuse == false)
		return;

	if(SND_MusicChannel.paused == false)
	{
		result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, true);
		FMOD_ERROR(result, true, false);

		SND_MusicChannel.paused = true;
	}

#else
	CDAudio_Pause();
#endif
}

void CDA_Resume (void)
{
#ifdef UQE_FMOD_CDAUDIO

	if(SND_InitialisedCD == false || SND_MusicChannel.inuse == false)
		return;

	if(SND_MusicChannel.paused == true)
	{
		result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, false);
		FMOD_ERROR(result, true, false);

		SND_MusicChannel.paused = false;
	}
#else
	CDAudio_Resume();
#endif
}

void CDA_Update (void)
{
#ifdef UQE_FMOD_CDAUDIO

	if(SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	FMOD_System_Update(fmod_system);
	SND_MusicChannel.volume = bgmvolume.value;

	if(SND_MusicChannel.volume < 0.0f)
		SND_MusicChannel.volume = 0.0f;

	if(SND_MusicChannel.volume > 1.0f)
		SND_MusicChannel.volume = 1.0f;

	FMOD_Channel_SetVolume(SND_MusicChannel.channel, SND_MusicChannel.volume);


	if(SND_MusicChannel.volume == 0.0f | cl.paused == true)
		CDA_Pause();
	else
		CDA_Resume();

#else
	CDAudio_Update();
#endif
}




// ===================================================================================
//
//  MOD AUDIO CONTROL ROUTINES (OGG / MP3 / WAV)
//
// ===================================================================================

void MOD_Start (char *name, qboolean midi, qboolean loop, qboolean notify, qboolean resume)
{
	char	file[MAX_QPATH];
	FMOD_CREATESOUNDEXINFO exinfo;

	if(SND_Initialised == false)
		return;

	if(SND_MusicChannel.inuse == true)
		FMOD_MusicStop();

	if(strlen(name) == 0)
		return;

	if(SND_FOpen(name, midi, resume) == true)
	{
		memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
		exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
		exinfo.length = SND_File.length;

		result = FMOD_System_CreateSound(fmod_system, (const char *)SND_File.data, FMOD_HARDWARE | FMOD_OPENMEMORY | FMOD_2D, &exinfo, &fmod_music);
		FMOD_ERROR(result, true, false);

		if(loop == true)
		{
			SND_MusicChannel.looping = true;
			result = FMOD_Sound_SetMode(fmod_music, FMOD_LOOP_NORMAL);
			FMOD_ERROR(result, true, false);
		}
		else
		{
			SND_MusicChannel.looping = false;
			result = FMOD_Sound_SetMode(fmod_music, FMOD_LOOP_OFF);
			FMOD_ERROR(result, true, false);
		}

		strcpy(file, SND_File.filename);
		SND_FClose();
	}


	if(!fmod_music)
	{
		Con_Printf("Couldn't open stream %s\n", file);
		return;
	}
	else
	{
		if(notify == true)
			Con_Printf("Playing: %s...\n", file);
	}

	result = FMOD_System_GetChannel(fmod_system, 0, &SND_MusicChannel.channel);
	FMOD_ERROR(result, true, false);

	result = FMOD_System_PlaySound(fmod_system, FMOD_CHANNEL_REUSE, fmod_music, 0, &SND_MusicChannel.channel);
    FMOD_ERROR(result, true, false);

	SND_MusicChannel.inuse = true;
}

void MOD_Stop (void)
{
	if(SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	if(SND_MusicChannel.channel)
	{
		result = FMOD_Channel_Stop(SND_MusicChannel.channel);
		FMOD_ERROR(result, true, false);
	}

	if(fmod_music)
	{
		result = FMOD_Sound_Release(fmod_music);
		FMOD_ERROR(result, true, false);
	}

	SND_MusicChannel.inuse = false;
}

void MOD_Pause (void)
{
	if(SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	if(SND_MusicChannel.paused == false)
	{
		result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, true);
		FMOD_ERROR(result, true, false);

		SND_MusicChannel.paused = true;
	}
}

void MOD_Resume (void)
{
	if(SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	if(SND_MusicChannel.paused == true)
	{
		result = FMOD_Channel_SetPaused(SND_MusicChannel.channel, false);
		FMOD_ERROR(result, true, false);

		SND_MusicChannel.paused = false;
	}
}

void MOD_Update (void)
{
	if(SND_Initialised == false || SND_MusicChannel.inuse == false)
		return;

	FMOD_System_Update(fmod_system);
	SND_MusicChannel.volume = bgmvolume.value;

	if(SND_MusicChannel.volume < 0.0f)
		SND_MusicChannel.volume = 0.0f;

	if(SND_MusicChannel.volume > 1.0f)
		SND_MusicChannel.volume = 1.0f;

	FMOD_Channel_SetVolume(SND_MusicChannel.channel, SND_MusicChannel.volume);

	if(SND_MusicChannel.volume == 0.0f | cl.paused == true)
		MOD_Pause();
	else
		MOD_Resume();
}




// ===================================================================================
//
//  MAIN FMOD AUDIO CONTROL ROUTINES
//
// ===================================================================================

void FMOD_MusicStart (char *name, byte track, qboolean loop, qboolean notify, qboolean resume)
{
	strcpy(oldbgmtype, bgmtype.string);

	if (strcmpi(bgmtype.string, "cd") == 0)
		CDA_Start(track, loop, notify);

	if (strcmpi(bgmtype.string, "mod") == 0)
		MOD_Start(name, false, loop, notify, resume);
}

void FMOD_MusicStartConsole (void)
{
	char name[MAX_QPATH];
	char file[MAX_QPATH];

	if (Cmd_Argc() < 2)
	{
		Con_Printf ("fmod_playmusic <filename> : play an audio file\n");
		return;
	}

	sprintf(name, "%s\0", Cmd_Argv(1));
	COM_StripExtension(name, file);

	MOD_Start(file, false, false, true, false);

	return;
}

void FMOD_MusicStop (void)
{
	CDA_Stop();
	MOD_Stop();
}

void FMOD_MusicPause (void)
{
	if (strcmpi(bgmtype.string, "cd") == 0)
		CDA_Pause();

	if (strcmpi(bgmtype.string, "mod") == 0)
		MOD_Pause();
}

void FMOD_MusicResume (void)
{
	if (strcmpi(bgmtype.string, "cd") == 0)
		CDA_Resume();

	if (strcmpi(bgmtype.string, "mod") == 0)
		MOD_Resume();
}

void FMOD_MusicUpdate (void)
{
	if (strcmpi(bgmtype.string, "cd") == 0)
		CDA_Update();

	if (strcmpi(bgmtype.string, "mod") == 0)
		MOD_Update();

	if (strcmpi(bgmtype.string, oldbgmtype) != 0)
	{
		FMOD_MusicStop();
		strcpy(oldbgmtype, bgmtype.string);
	}
}
