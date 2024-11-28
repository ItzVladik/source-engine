#include "cbase.h"
#include "AloneMod/AmodCvars.h"
#include "AloneMod/Amod_SharedDefs.h"
#include "movevars_shared.h"
#include "filesystem.h"
#include "fmtstr.h"
#include "engine/IEngineSound.h"

extern ConVar r_flashlightfar;
extern ConVar r_flashlightfov;
extern ConVar amod_day;

//use keyvalues: X	  
// 
//use sstream:		  /
//					\/
// 
// 
//should have used keyvalues:		/
//								   /
//								 \/

CON_COMMAND_F(Amod_WriteConfig, "", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN)
{
	FileHandle_t file = filesystem->Open("cfg/AloneMod_Config.txt", "wb", "MOD");
	if (!file)
	{
		ConWarning("Failed To Open Or Make AloneMod_Config.cfg\n");
		return;
	}

	ConVarRef flag("amod_flashlightlag");
	ConVarRef fflicker("amod_flashlightflicker");
	ConVarRef mirrored("amod_mirrored");
	ConVarRef amod_soundscapes_disable("amod_soundscapes_disable");
	ConVarRef amod_music_disable_tmp("amod_music_disable");
	ConVarRef sv_footsteps("sv_footsteps");
	ConVarRef cl_drawhud("cl_drawhud");
	ConVarRef amod_enabl_god("amod_enable_god");
	ConVarRef amod_standbob_enabled("amod_standbob_enabled");

	char buf[800];
	Q_snprintf(buf, sizeof(buf), "amod_viewbob_enabled %d\nsv_rollangle %d\namod_jump_punch_enable %d\namod_land_punch_enable %d\nr_flashlightfar %d\nr_flashlightfov %d\nfilter_brightness_on %d\nfilter_brightness_on_exp %d\nfilter_brightness_off %d\namod_flashlightlag %d\namod_mirrored %d\namod_flashlightflicker %d\namod_soundscapes_disable %d\namod_music_disable %d\nsv_footsteps %d\ncl_drawhud %d\namod_fog_disabled %d\namod_enable_god %d\namod_standbob_enabled %d\namod_day %d\namod_vignette %d",
		amod_viewbob_enabled.GetInt(), sv_rollangle.GetInt(), amod_jump_punch_enable.GetInt(), amod_land_punch_enable.GetInt(), r_flashlightfar.GetInt(), r_flashlightfov.GetInt(), amod_filter_on_brightness.GetInt(), amod_filter_on_exp.GetInt(), amod_filter_off_brightness.GetInt(), 
		flag.GetBool(), mirrored.GetBool(), fflicker.GetBool(), amod_soundscapes_disable.GetBool(), amod_music_disable_tmp.GetBool(), sv_footsteps.GetBool(), cl_drawhud.GetBool(), amod_fog_disabled.GetBool(), amod_enabl_god.GetInt(), amod_standbob_enabled.GetBool(), amod_day.GetBool(), amod_vignette.GetBool());

	filesystem->Write(buf, Q_strlen(buf), file);
	filesystem->Close(file);
}

CON_COMMAND_F(map_random, "Goes to a random map", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
	int i = random->RandomInt(0, (sizeof(g_szMapNames) / sizeof(g_szMapNames[0]) - 1));
	engine->ClientCmd_Unrestricted(CFmtStr("map %s", g_szMapNames[i]));
}

CON_COMMAND(amod_rain_stopsounds, "stops rain sounds")
{
	CUtlVector<SndInfo_t> vec;
	enginesound->GetActiveSounds(vec);
	for (int i = 0; i < vec.Count(); i++)
	{
		SndInfo_t snd = vec[i];
		char buffer[512];
		filesystem->String(snd.m_filenameHandle, buffer, sizeof(buffer));
		if (Q_strstr(buffer, "ambient\\weather\\rain"))
			enginesound->StopSoundByGuid(snd.m_nGuid);
	}
}