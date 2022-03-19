#include <base_feature.h>
#include <steamtypes.h>

#include <convar.h>

#include <IMemoryUtils.h>
#include <IDetoursAPI.h>
#include <IPlayerUtils.h>
#include <ISvenModAPI.h>

#include <client_state.h>
#include <usermessages.h>
#include <memutils/patterns.h>
#include <data_struct/hashtable.h>

//-----------------------------------------------------------------------------
// Patterns
//-----------------------------------------------------------------------------

DEFINE_PATTERN(CHudBaseTextBlock__Print, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 53 56 57 A1 ? ? ? ? 33 C5 50 8D 45 F4 64 A3 00 00 00 00 8B D9 8B 0D");

DEFINE_PATTERN(CVoiceBanMgr__SetPlayerBan, "56 FF 74 24 08 8B F1 E8 ? ? ? ? 80 7C 24 0C 00 74 13 85 C0 75 32 FF 74 24 08 8B CE E8");
DEFINE_PATTERN(CVoiceBanMgr__InternalFindPlayerSquelch, "53 55 8B 6C 24 0C 56 57 0F 10 4D 00 0F 28 C1 66 0F 73 D8 08 66 0F FC C8 0F 10 C1 66 0F 73 D8 04");

DEFINE_PATTERN(CVoiceStatus__IsPlayerBlocked, "83 EC 14 A1 ? ? ? ? 33 C4 89 44 24 10 56 8D 44 24 04 8B F1 50 FF 74 24 20 FF 15");
DEFINE_PATTERN(CVoiceStatus__SetPlayerBlockedState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 68 ? ? ? ? 8B D9 FF 15 ? ? ? ? D9 5C 24 08");
DEFINE_PATTERN(CVoiceStatus__UpdateServerState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 8B D9 89 5C 24 08");

DEFINE_PATTERN(HACK_GetPlayerUniqueID, "FF 74 24 08 FF 74 24 08 FF 15 ? ? ? ? 83 C4 08 85 C0 0F 95 C0 C3");

//-----------------------------------------------------------------------------
// Declare hooks
//-----------------------------------------------------------------------------

DECLARE_CLASS_HOOK(void, CHudBaseTextBlock__Print, void *, uintptr_t, int, int);

DECLARE_CLASS_HOOK(void, CVoiceBanMgr__SetPlayerBan, void *, char *, bool);
DECLARE_CLASS_HOOK(void *, CVoiceBanMgr__InternalFindPlayerSquelch, void *, char *);

DECLARE_CLASS_HOOK(bool, CVoiceStatus__IsPlayerBlocked, void *, int);
DECLARE_CLASS_HOOK(void, CVoiceStatus__SetPlayerBlockedState, void *, int, bool);
DECLARE_CLASS_HOOK(void, CVoiceStatus__UpdateServerState, void *, bool);

DECLARE_HOOK(bool, __cdecl, HACK_GetPlayerUniqueID, int, char *);

//-----------------------------------------------------------------------------
// Macro definitions
//-----------------------------------------------------------------------------

// Mute flags
#define MUTE_NONE ( 0 )
#define MUTE_VOICE ( 0x10 )
#define MUTE_CHAT ( 0x20 )
#define MUTE_ALL ( MUTE_VOICE | MUTE_CHAT )

// Muted players container specifics
#define IMM_VERSION ( 1 )
#define IMM_HEADER ( 0x2F77 )

//-----------------------------------------------------------------------------
// Vars
//-----------------------------------------------------------------------------

CHashTable<uint64, uint32> g_MutedPlayers(255);

bool g_bPaused = false;
bool g_bProcessingChat = false;
int g_nLastIndexedPlayer = -1;
uint32 g_BanMask = 0;

cvar_t *voice_clientdebug = NULL;
cvar_t *voice_modenable = NULL;

ConVar imm_mute_all_communications("imm_mute_all_communications", "0", FCVAR_CLIENTDLL, "If you muted a player, all (voice and chat) communications of this player will be muted");
ConVar imm_autosave_to_file("imm_autosave_to_file", "1", FCVAR_CLIENTDLL, "Automatically save muted players to the file \"muted_players.bin\"");

DetourHandle_t hCHudBaseTextBlock__Print = 0;
DetourHandle_t hCVoiceBanMgr__SetPlayerBan = 0;
DetourHandle_t hCVoiceBanMgr__InternalFindPlayerSquelch = 0;
DetourHandle_t hCVoiceStatus__IsPlayerBlocked = 0;
DetourHandle_t hCVoiceStatus__SetPlayerBlockedState = 0;
DetourHandle_t hCVoiceStatus__UpdateServerState = 0;
DetourHandle_t hHACK_GetPlayerUniqueID = 0;

UserMsgHookFn ORIG_UserMsgHook_SayText = NULL;
DetourHandle_t hUserMsgHook_SayText = 0;

//-----------------------------------------------------------------------------
// Purpose: load muted players in hash table from file muted_players.bin
//-----------------------------------------------------------------------------

void LoadMutedPlayers()
{
	FILE *file = fopen("muted_players.bin", "rb");

	if (file)
	{
		int buffer = 0;

		fread(&buffer, 1, sizeof(short), file);

		if (buffer != IMM_HEADER)
		{
			Warning("[Improved Mute Manager] Invalid header of file \"muted_players.bin\"\n");
			return;
		}

		buffer = 0;
		fread(&buffer, 1, sizeof(char), file);

		if (buffer < 1)
		{
			Warning("[Improved Mute Manager] Invalid version of file \"muted_players.bin\"\n");
			return;
		}

		static struct MutedPlayerEntry
		{
			uint32 steamid_high;
			uint32 steamid_low;
			uint32 flags;
		} s_MutedPlayerBuffer;

		while (fread(&s_MutedPlayerBuffer, 1, sizeof(MutedPlayerEntry), file) == sizeof(MutedPlayerEntry))
		{
			uint64 steamid = *reinterpret_cast<uint64_t *>(&s_MutedPlayerBuffer.steamid_high);

			g_MutedPlayers.Insert(steamid, s_MutedPlayerBuffer.flags);
		}

		fclose(file);
	}
	else
	{
		Warning("[Improved Mute Manager] Missing file \"muted_players.bin\"\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: save hash table in file muted_players.bim
//-----------------------------------------------------------------------------

void SaveMutedPlayers()
{
	FILE *file = fopen("muted_players.bin", "wb");

	if (file)
	{
		int buffer = 0;
		int saved_players = 0;

		buffer = IMM_HEADER;
		fwrite(&buffer, 1, sizeof(short), file);

		buffer = IMM_VERSION;
		fwrite(&buffer, 1, sizeof(char), file);

		for (int i = 0; i < g_MutedPlayers.Count(); i++)
		{
			HashTableIterator_t it = g_MutedPlayers.First(i);

			while (g_MutedPlayers.IsValidIterator(it))
			{
				uint64 &steamid = g_MutedPlayers.KeyAt(i, it);
				uint32 &mute_flags = g_MutedPlayers.ValueAt(i, it);

				fwrite(&steamid, 1, sizeof(uint64), file);
				fwrite(&mute_flags, 1, sizeof(uint32), file);

				it = g_MutedPlayers.Next(i, it);
				++saved_players;
			}
		}

		Msg("[Improved Mute Manager] Saved %d players in file \"muted_players.bin\"\n", saved_players);

		fclose(file);
	}
	else
	{
		Warning("[Improved Mute Manager] Cannot create file \"muted_players.bin\"\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: remove all muted players from hash table and clear it
//-----------------------------------------------------------------------------

void RemoveMutedPlayers()
{
	g_MutedPlayers.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: get muted player from hash table
//-----------------------------------------------------------------------------

FORCEINLINE uint32 *GetMutedPlayer(uint64 steamid)
{
	return g_MutedPlayers.Find(steamid);
}

//-----------------------------------------------------------------------------
// Purpose: add player in hash table or merge flags if player already in table
//-----------------------------------------------------------------------------

static void OnPlayerFound(uint32 *pFoundValue, uint32 *pInsertValue)
{
	*pFoundValue |= *pInsertValue;
}

FORCEINLINE bool AddMutedPlayer(uint64 steamid, uint32 flags)
{
	return g_MutedPlayers.Insert(steamid, flags, OnPlayerFound);
}

//-----------------------------------------------------------------------------
// Purpose: remove player from hash table if resulting flag is MUTE_NONE
//-----------------------------------------------------------------------------

static bool OnPlayerRemove(uint32 *pRemoveValue, uint32 *pUserValue)
{
	*pRemoveValue &= ~(*pUserValue);

	if (*pRemoveValue != MUTE_NONE)
		return false;

	return true;
}

FORCEINLINE bool RemoveMutedPlayer(uint64 steamid, uint32 flags)
{
	return g_MutedPlayers.Remove(steamid, OnPlayerRemove, &flags);
}

//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------

#define IMM_CHECK_PLAYER(idx) \
	idx > 0 && idx <= MAXCLIENTS && \
	SvenModAPI()->GetClientState() == CLS_ACTIVE && \
	SvenModAPI()->EngineFuncs()->GetEntityByIndex(idx) && \
	SvenModAPI()->EngineFuncs()->GetEntityByIndex(idx) != SvenModAPI()->EngineFuncs()->GetLocalPlayer()

CON_COMMAND(imm_mute_voice, "Mute player's voice communication")
{
	if ( g_bPaused )
		return;

	if (args.ArgC() > 1)
	{
		int nPlayerIndex = atoi(args[1]);

		if ( IMM_CHECK_PLAYER(nPlayerIndex) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

			AddMutedPlayer(steamid, MUTE_VOICE);

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo(nPlayerIndex - 1);

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg("[Improved Mute Manager] Player \"%s\" muted (voice)\n", pPlayerInfo->name);
		}
	}
	else
	{
		ConMsg("Usage:  imm_mute_voice <player index>\n");
	}
}

CON_COMMAND(imm_mute_chat, "Mute player's chat communication")
{
	if ( g_bPaused )
		return;

	if (args.ArgC() > 1)
	{
		int nPlayerIndex = atoi(args[1]);

		if ( IMM_CHECK_PLAYER(nPlayerIndex) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

			AddMutedPlayer(steamid, MUTE_CHAT);

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo(nPlayerIndex - 1);

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg("[Improved Mute Manager] Player \"%s\" muted (chat)\n", pPlayerInfo->name);
		}
	}
	else
	{
		ConMsg("Usage:  imm_mute_chat <player index>\n");
	}
}

CON_COMMAND(imm_mute_all, "Mute all player communications")
{
	if ( g_bPaused )
		return;

	if (args.ArgC() > 1)
	{
		int nPlayerIndex = atoi(args[1]);

		if ( IMM_CHECK_PLAYER(nPlayerIndex) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

			AddMutedPlayer(steamid, MUTE_ALL);

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo(nPlayerIndex - 1);

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg("[Improved Mute Manager] Player \"%s\" muted\n", pPlayerInfo->name);
		}
	}
	else
	{
		ConMsg("Usage:  imm_mute_all <player index>\n");
	}
}

CON_COMMAND(imm_unmute_voice, "Unmute player's voice communication")
{
	if ( g_bPaused )
		return;

	if (args.ArgC() > 1)
	{
		int nPlayerIndex = atoi(args[1]);

		if ( IMM_CHECK_PLAYER(nPlayerIndex) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

			RemoveMutedPlayer(steamid, MUTE_VOICE);

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo(nPlayerIndex - 1);

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg("[Improved Mute Manager] Player \"%s\" unmuted (voice)\n", pPlayerInfo->name);
		}
	}
	else
	{
		ConMsg("Usage:  imm_unmute_voice <player index>\n");
	}
}

CON_COMMAND(imm_unmute_chat, "Unmute player's chat communication")
{
	if ( g_bPaused )
		return;

	if (args.ArgC() > 1)
	{
		int nPlayerIndex = atoi(args[1]);

		if ( IMM_CHECK_PLAYER(nPlayerIndex) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

			RemoveMutedPlayer(steamid, MUTE_CHAT);

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo(nPlayerIndex - 1);

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg("[Improved Mute Manager] Player \"%s\" unmuted (chat)\n", pPlayerInfo->name);
		}
	}
	else
	{
		ConMsg("Usage:  imm_unmute_chat <player index>\n");
	}
}

CON_COMMAND(imm_unmute_all, "Unmute all player communications")
{
	if ( g_bPaused )
		return;

	if (args.ArgC() > 1)
	{
		int nPlayerIndex = atoi(args[1]);

		if ( IMM_CHECK_PLAYER(nPlayerIndex) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

			RemoveMutedPlayer(steamid, MUTE_ALL);

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo(nPlayerIndex - 1);

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg("[Improved Mute Manager] Player \"%s\" unmuted\n", pPlayerInfo->name);
		}
	}
	else
	{
		ConMsg("Usage:  imm_unmute_all <player index>\n");
	}
}

CON_COMMAND(imm_unmute_by_steamid64, "Unmute all player communications with given Steam64 ID")
{
	if ( g_bPaused )
		return;

	if (args.ArgC() > 1)
	{
		uint64 steamid = atoll(args[1]);

		if (RemoveMutedPlayer(steamid, MUTE_ALL))
		{
			Msg("[Improved Mute Manager] SteamID %llu has been removed\n", steamid);
		}
		else
		{
			Msg("[Improved Mute Manager] SteamID %llu not found\n", steamid);
		}
	}
	else
	{
		ConMsg("Usage:  imm_unmute_by_steamid64 <Steam64 ID>\n");
	}
}

CON_COMMAND(imm_save_to_file, "Save all muted players to file \"muted_players.bin\"")
{
	if ( g_bPaused )
		return;

	SaveMutedPlayers();
}

CON_COMMAND(imm_print_muted_players, "Print all muted players")
{
	if ( g_bPaused )
		return;

	Msg("====================== Muted Players ======================\n");

	int current_players = 1;

	for (int i = 0; i < g_MutedPlayers.Count(); i++)
	{
		HashTableIterator_t it = g_MutedPlayers.First(i);

		while (g_MutedPlayers.IsValidIterator(it))
		{
			uint64 &steamid = g_MutedPlayers.KeyAt(i, it);
			uint32 &mute_flags = g_MutedPlayers.ValueAt(i, it);

			Msg("%d >> SteamID: %llu | Voice: %d | Chat: %d\n", current_players, steamid, (mute_flags & MUTE_VOICE) != 0, (mute_flags & MUTE_CHAT) != 0);

			it = g_MutedPlayers.Next(i, it);
			++current_players;
		}
	}

	Msg("====================== Muted Players ======================\n");
}

CON_COMMAND(imm_print_current_muted_players, "Print currently muted players")
{
	if ( g_bPaused )
		return;

	if ( SvenModAPI()->GetClientState() != CLS_ACTIVE )
		return;

	Msg("====================== Muted Players ======================\n");

	cl_entity_t *pLocal = SvenModAPI()->EngineFuncs()->GetLocalPlayer();
	int nLocalPlayer = pLocal->index;

	for (int i = 1; i <= MAXCLIENTS; ++i)
	{
		if ( i == nLocalPlayer )
			continue;

		uint64 steamid = PlayerUtils()->GetSteamID(i);

		if ( !steamid )
			continue;

		uint32 *mute_flags = GetMutedPlayer(steamid);

		if ( !mute_flags )
			continue;

		player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo(i - 1);

		Msg("#%d >> Player: \"%s\" | Voice: %d | Chat: %d\n", i, pPlayerInfo->name, (*mute_flags & MUTE_VOICE) != 0, (*mute_flags & MUTE_CHAT) != 0);
	}

	Msg("====================== Muted Players ======================\n");
}

//-----------------------------------------------------------------------------
// Hooks
//-----------------------------------------------------------------------------

// Called when you receive messages to print in the chat

/*
DECLARE_CLASS_FUNC(void, HOOKED_CHudBaseTextBlock__Print, void *thisptr, uintptr_t a1, int a2, int a3)
{
	g_bProcessingChat = true;

	ORIG_CHudBaseTextBlock__Print(thisptr, a1, a2, a3);

	g_bProcessingChat = false;
}
*/

DECLARE_CLASS_FUNC(void, HOOKED_CVoiceBanMgr__SetPlayerBan, void *thisptr, char *pszPlayerUniqueID, bool bMute)
{
	uint64 steamid = PlayerUtils()->GetSteamID(g_nLastIndexedPlayer);

	if ( !steamid )
		return;

	if ( bMute )
		AddMutedPlayer(steamid, MUTE_VOICE);
	else
		RemoveMutedPlayer(steamid, MUTE_VOICE);
}

// Get pointer to muted player (won't hook CVoiceBanMgr::GetPlayerBan to save perfomance speed)

DECLARE_CLASS_FUNC(void *, HOOKED_CVoiceBanMgr__InternalFindPlayerSquelch, void *thisptr, char *pszPlayerUniqueID)
{
	uint64 steamid = PlayerUtils()->GetSteamID(g_nLastIndexedPlayer);

	if ( !steamid )
		return NULL;

	uint32 *mute_flags = GetMutedPlayer(steamid);

	if ( mute_flags && ( *mute_flags & MUTE_VOICE || imm_mute_all_communications.GetBool() ) )
		return mute_flags;

	return NULL;
}

DECLARE_CLASS_FUNC(bool, HOOKED_CVoiceStatus__IsPlayerBlocked, void *thisptr, int nPlayerIndex)
{
	uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

	if ( !steamid )
		return false;

	uint32 *mute_flags = GetMutedPlayer(steamid);

	if ( mute_flags )
	{
		if ( imm_mute_all_communications.GetBool() )
			return true;

		if ( g_bProcessingChat )
		{
			if ( *mute_flags & MUTE_CHAT )
				return true;
		}
		else if ( *mute_flags & MUTE_VOICE )
		{
			return true;
		}
	}

	return false;
}

// Called when you (un)mute player via scoreboard

DECLARE_CLASS_FUNC(void, HOOKED_CVoiceStatus__SetPlayerBlockedState, void *thisptr, int nPlayerIndex, bool bMute)
{
	uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

	if ( !steamid )
		return;

	if ( bMute)
		AddMutedPlayer(steamid, MUTE_VOICE);
	else
		RemoveMutedPlayer(steamid, MUTE_VOICE);
}

// Send to server the mask of muted players that we don't want to hear

DECLARE_CLASS_FUNC(void, HOOKED_CVoiceStatus__UpdateServerState, void *thisptr, bool bForce)
{
	//if ( SvenModAPI()->GetClientState() != CLS_ACTIVE )
	//	return;

	static float flForceBanMaskTime = 0.f;
	static char command_buffer[128];

	char const *pLevelName = SvenModAPI()->EngineFuncs()->GetLevelName();
	bool bClientDebug = bool(voice_clientdebug->value);

	if ( *pLevelName == 0 && bClientDebug )
	{
		Msg("CVoiceStatus::UpdateServerState: pLevelName[0]==0\n");
		return;
	}

	uint32 banMask = 0;

	bool bMuteEverything = imm_mute_all_communications.GetBool();
	bool bVoiceModEnable = static_cast<bool>(voice_modenable->value);

	// thisptr members
	float *m_LastUpdateServerState = (float *)((unsigned char *)thisptr + 0x18);
	int *m_bServerModEnable = (int *)((unsigned char *)thisptr + 0x1C);

	// validate cvar 'voice_modenable'
	if ( bForce || bool(*m_bServerModEnable) != bVoiceModEnable )
	{
		*m_bServerModEnable = static_cast<int>(bVoiceModEnable);

		_snprintf(command_buffer, sizeof(command_buffer), "VModEnable %d", bVoiceModEnable);
		SvenModAPI()->EngineFuncs()->ClientCmd(command_buffer);

		command_buffer[sizeof(command_buffer) - 1] = 0;

		if ( bClientDebug )
			Msg("CVoiceStatus::UpdateServerState: Sending '%s'\n", command_buffer);
	}

	// build ban mask
	for (uint32 i = 1; i <= MAXCLIENTS; ++i)
	{
		uint64 steamid = PlayerUtils()->GetSteamID(i);

		if ( !steamid )
			continue;

		uint32 *mute_flags = GetMutedPlayer(steamid);

		if ( mute_flags && ( bMuteEverything || *mute_flags & MUTE_VOICE ) )
			banMask |= 1 << (i - 1); // one bit, one client
	}

	if ( SvenModAPI()->EngineFuncs()->GetClientTime() - flForceBanMaskTime < 0.f )
	{
		flForceBanMaskTime = 0.f;
	}

	if ( g_BanMask != banMask || (SvenModAPI()->EngineFuncs()->GetClientTime() - flForceBanMaskTime >= 5.0f) )
	{
		_snprintf(command_buffer, sizeof(command_buffer), "vban %X", banMask); // vban [ban_mask]

		if ( bClientDebug )
			Msg("CVoiceStatus::UpdateServerState: Sending '%s'\n", command_buffer);

		SvenModAPI()->EngineFuncs()->ClientCmd(command_buffer);
		g_BanMask = banMask;
	}
	else if ( bClientDebug )
	{
		Msg("CVoiceStatus::UpdateServerState: no change\n");
	}

	*m_LastUpdateServerState = flForceBanMaskTime = SvenModAPI()->EngineFuncs()->GetClientTime();
}

DECLARE_FUNC(bool, __cdecl, HOOKED_HACK_GetPlayerUniqueID, int nPlayerIndex, char *pszPlayerUniqueID)
{
	g_nLastIndexedPlayer = nPlayerIndex;
	return ORIG_HACK_GetPlayerUniqueID(nPlayerIndex, pszPlayerUniqueID);
}

int UserMsgHook_SayText(const char *pszName, int iSize, void *pBuffer)
{
	UserMessages::BeginRead(pBuffer, iSize);

	int result = 0;
	int nPlayerIndex = UserMessages::ReadByte();

	uint64 steamid = PlayerUtils()->GetSteamID(nPlayerIndex);

	if ( !steamid )
	{
		g_bProcessingChat = true;

		result = ORIG_UserMsgHook_SayText(pszName, iSize, pBuffer);

		g_bProcessingChat = false;

		return result;
	}

	uint32 *mute_flags = GetMutedPlayer(steamid);

	if ( mute_flags )
	{
		if ( imm_mute_all_communications.GetBool() || *mute_flags & MUTE_CHAT )
			return 0;
	}

	g_bProcessingChat = true;

	result = ORIG_UserMsgHook_SayText(pszName, iSize, pBuffer);

	g_bProcessingChat = false;

	return result;
}

//-----------------------------------------------------------------------------
// Control class
//-----------------------------------------------------------------------------

bool LoadMuteManager()
{
	// Find signatures

	/*
	void *pCHudBaseTextBlock__Print = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, CHudBaseTextBlock__Print );

	if ( !pCHudBaseTextBlock__Print )
	{
		Warning("[Improved Mute Manager] Can't find function CHudBaseTextBlock::Print\n");
		return false;
	}
	*/

	void *pCVoiceBanMgr__SetPlayerBan = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, CVoiceBanMgr__SetPlayerBan );

	if ( !pCVoiceBanMgr__SetPlayerBan )
	{
		Warning("[Improved Mute Manager] Can't find function CVoiceBanMgr::SetPlayerBan\n");
		return false;
	}

	void *pCVoiceBanMgr__InternalFindPlayerSquelch = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, CVoiceBanMgr__InternalFindPlayerSquelch );

	if ( !pCVoiceBanMgr__InternalFindPlayerSquelch )
	{
		Warning("[Improved Mute Manager] Can't find function CVoiceBanMgr::InternalFindPlayerSquelch\n");
		return false;
	}

	void *pCVoiceStatus__IsPlayerBlocked = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, CVoiceStatus__IsPlayerBlocked );

	if ( !pCVoiceStatus__IsPlayerBlocked )
	{
		Warning("[Improved Mute Manager] Can't find function CVoiceStatus::IsPlayerBlocked\n");
		return false;
	}

	void *pCVoiceStatus__SetPlayerBlockedState = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, CVoiceStatus__SetPlayerBlockedState );

	if ( !pCVoiceStatus__SetPlayerBlockedState )
	{
		Warning("[Improved Mute Manager] Can't find function CVoiceStatus::SetPlayerBlockedState\n");
		return false;
	}

	void *pCVoiceStatus__UpdateServerState = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, CVoiceStatus__UpdateServerState );

	if ( !pCVoiceStatus__UpdateServerState )
	{
		Warning("[Improved Mute Manager] Can't find function CVoiceStatus::UpdateServerState\n");
		return false;
	}

	void *pHACK_GetPlayerUniqueID = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, HACK_GetPlayerUniqueID );

	if ( !pHACK_GetPlayerUniqueID )
	{
		Warning("[Improved Mute Manager] Can't find function HACK_GetPlayerUniqueID\n");
		return false;
	}

	// Get native cvars

	voice_clientdebug = SvenModAPI()->CVar()->FindCvar("voice_clientdebug");

	if ( !voice_clientdebug )
	{
		Warning("[Improved Mute Manager] Can't find cvar \"voice_clientdebug\"\n");
		return false;
	}

	voice_modenable = SvenModAPI()->CVar()->FindCvar("voice_modenable");

	if ( !voice_modenable )
	{
		Warning("[Improved Mute Manager] Can't find cvar \"voice_modenable\"\n");
		return false;
	}

	LoadMutedPlayers();
	ConVar_Register();

	// Hook functions

	//hCHudBaseTextBlock__Print = DetoursAPI()->DetourFunction( pCHudBaseTextBlock__Print, HOOKED_CHudBaseTextBlock__Print, GET_FUNC_PTR(ORIG_CHudBaseTextBlock__Print) );
	hCVoiceBanMgr__SetPlayerBan = DetoursAPI()->DetourFunction( pCVoiceBanMgr__SetPlayerBan, HOOKED_CVoiceBanMgr__SetPlayerBan, GET_FUNC_PTR(ORIG_CVoiceBanMgr__SetPlayerBan) );
	hCVoiceBanMgr__InternalFindPlayerSquelch = DetoursAPI()->DetourFunction( pCVoiceBanMgr__InternalFindPlayerSquelch, HOOKED_CVoiceBanMgr__InternalFindPlayerSquelch, GET_FUNC_PTR(ORIG_CVoiceBanMgr__InternalFindPlayerSquelch) );
	hCVoiceStatus__IsPlayerBlocked = DetoursAPI()->DetourFunction( pCVoiceStatus__IsPlayerBlocked, HOOKED_CVoiceStatus__IsPlayerBlocked, GET_FUNC_PTR(ORIG_CVoiceStatus__IsPlayerBlocked) );
	hCVoiceStatus__SetPlayerBlockedState = DetoursAPI()->DetourFunction( pCVoiceStatus__SetPlayerBlockedState, HOOKED_CVoiceStatus__SetPlayerBlockedState, GET_FUNC_PTR(ORIG_CVoiceStatus__SetPlayerBlockedState) );
	hCVoiceStatus__UpdateServerState = DetoursAPI()->DetourFunction( pCVoiceStatus__UpdateServerState, HOOKED_CVoiceStatus__UpdateServerState, GET_FUNC_PTR(ORIG_CVoiceStatus__UpdateServerState) );
	hHACK_GetPlayerUniqueID = DetoursAPI()->DetourFunction( pHACK_GetPlayerUniqueID, HOOKED_HACK_GetPlayerUniqueID, GET_FUNC_PTR(ORIG_HACK_GetPlayerUniqueID) );

	hUserMsgHook_SayText = Hooks()->HookUserMessage( "SayText", UserMsgHook_SayText, &ORIG_UserMsgHook_SayText );

	return true;
}

void UnloadMuteManager()
{
	if ( imm_autosave_to_file.GetBool() )
		SaveMutedPlayers();

	RemoveMutedPlayers();

	//DetoursAPI()->RemoveDetour( hCHudBaseTextBlock__Print );
	DetoursAPI()->RemoveDetour( hCVoiceBanMgr__SetPlayerBan );
	DetoursAPI()->RemoveDetour( hCVoiceBanMgr__InternalFindPlayerSquelch );
	DetoursAPI()->RemoveDetour( hCVoiceStatus__IsPlayerBlocked );
	DetoursAPI()->RemoveDetour( hCVoiceStatus__SetPlayerBlockedState );
	DetoursAPI()->RemoveDetour( hCVoiceStatus__UpdateServerState );
	DetoursAPI()->RemoveDetour( hHACK_GetPlayerUniqueID );

	Hooks()->UnhookUserMessage( hUserMsgHook_SayText );

	ConVar_Unregister();
}

void PauseMuteManager()
{
	g_bPaused = true;

	//DetoursAPI()->PauseDetour( hCHudBaseTextBlock__Print );
	DetoursAPI()->PauseDetour( hCVoiceBanMgr__SetPlayerBan );
	DetoursAPI()->PauseDetour( hCVoiceBanMgr__InternalFindPlayerSquelch );
	DetoursAPI()->PauseDetour( hCVoiceStatus__IsPlayerBlocked );
	DetoursAPI()->PauseDetour( hCVoiceStatus__SetPlayerBlockedState );
	DetoursAPI()->PauseDetour( hCVoiceStatus__UpdateServerState );
	DetoursAPI()->PauseDetour( hHACK_GetPlayerUniqueID );

	Hooks()->UnhookUserMessage( hUserMsgHook_SayText );
}

void UnpauseMuteManager()
{
	g_bPaused = false;

	//DetoursAPI()->UnpauseDetour( hCHudBaseTextBlock__Print );
	DetoursAPI()->UnpauseDetour( hCVoiceBanMgr__SetPlayerBan );
	DetoursAPI()->UnpauseDetour( hCVoiceBanMgr__InternalFindPlayerSquelch );
	DetoursAPI()->UnpauseDetour( hCVoiceStatus__IsPlayerBlocked );
	DetoursAPI()->UnpauseDetour( hCVoiceStatus__SetPlayerBlockedState );
	DetoursAPI()->UnpauseDetour( hCVoiceStatus__UpdateServerState );
	DetoursAPI()->UnpauseDetour( hHACK_GetPlayerUniqueID );

	hUserMsgHook_SayText = Hooks()->HookUserMessage( "SayText", UserMsgHook_SayText, &ORIG_UserMsgHook_SayText );
}