// C++
// Engine Module

#include "engine.h"

//-----------------------------------------------------------------------------
// "Interfaces"
//-----------------------------------------------------------------------------

extern cl_enginefunc_t *g_pEngineFuncs;
extern cl_clientfunc_t *g_pClientFuncs;
extern engine_studio_api_t *g_pEngineStudio;
extern r_studio_interface_t *g_pStudioAPI;

//-----------------------------------------------------------------------------
// Vars
//-----------------------------------------------------------------------------

static bool __INITIALIZED__ = false;
int g_nLastIndexedPlayer = -1;

//-----------------------------------------------------------------------------
// Original functions
//-----------------------------------------------------------------------------

FnGetPlayerUniqueID GetPlayerUniqueID_Original = NULL;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Hooks
//-----------------------------------------------------------------------------

qboolean GetPlayerUniqueID_Hooked(int iPlayer, char playerID[16])
{
	g_nLastIndexedPlayer = iPlayer;
	return GetPlayerUniqueID_Original(iPlayer, playerID);
}

//-----------------------------------------------------------------------------
// Init/release engine module
//-----------------------------------------------------------------------------

bool InitEngineModule()
{
	GetPlayerUniqueID_Original = g_pEngineFuncs->GetPlayerUniqueID;
	g_pEngineFuncs->GetPlayerUniqueID = GetPlayerUniqueID_Hooked;

	__INITIALIZED__ = true;
	return true;
}

void ReleaseEngineModule()
{
	if (!__INITIALIZED__)
		return;

	g_pEngineFuncs->GetPlayerUniqueID = GetPlayerUniqueID_Original;
}