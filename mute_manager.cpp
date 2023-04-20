#include <steamtypes.h>

#include <convar.h>

#include <IMemoryUtils.h>
#include <IPlayerUtils.h>
#include <ISvenModAPI.h>

#include <client_state.h>
#include <messagebuffer.h>
#include <memutils/patterns.h>
#include <data_struct/hashtable.h>

#include "mute_manager.h"
#include "patterns.h"

//-----------------------------------------------------------------------------
// Declare hooks
//-----------------------------------------------------------------------------

DECLARE_CLASS_HOOK( void, CHudBaseTextBlock__Print, void *, uintptr_t, int, int );

DECLARE_CLASS_HOOK( void, CVoiceBanMgr__SetPlayerBan, void *, char *, bool );
DECLARE_CLASS_HOOK( void *, CVoiceBanMgr__InternalFindPlayerSquelch, void *, char * );

DECLARE_CLASS_HOOK( bool, CVoiceStatus__IsPlayerBlocked, void *, int );
DECLARE_CLASS_HOOK( void, CVoiceStatus__SetPlayerBlockedState, void *, int, bool );
DECLARE_CLASS_HOOK( void, CVoiceStatus__UpdateServerState, void *, bool );

DECLARE_HOOK( bool, __cdecl, HACK_GetPlayerUniqueID, int, char * );

//-----------------------------------------------------------------------------
// Macro definitions
//-----------------------------------------------------------------------------

// Muted players container specifics
#define IMM_VERSION ( 1 )
#define IMM_HEADER ( 0x2F77 )

//-----------------------------------------------------------------------------
// Vars
//-----------------------------------------------------------------------------

CHashTable<uint64, uint32> g_MutedPlayers( 255 );

bool g_bPaused = false;
bool g_bProcessingChat = false;
int g_nLastIndexedPlayer = -1;
uint32 g_BanMask = 0;

cvar_t *voice_clientdebug = NULL;
cvar_t *voice_modenable = NULL;

ConVar imm_mute_all_communications( "imm_mute_all_communications", "0", FCVAR_CLIENTDLL, "If you muted a player, all (voice and chat) communications of this player will be muted" );
ConVar imm_autosave_to_file( "imm_autosave_to_file", "1", FCVAR_CLIENTDLL, "Automatically save muted players to the file \"muted_players.bin\"" );

CMessageBuffer SayTextBuffer;
UserMsgHookFn ORIG_UserMsgHook_SayText = NULL;

CMuteManager g_MuteManager;

//-----------------------------------------------------------------------------
// Purpose: load muted players in hash table from file muted_players.bin
//-----------------------------------------------------------------------------

void LoadMutedPlayers()
{
	FILE *file = fopen( "muted_players.bin", "rb" );

	if ( file )
	{
		int buffer = 0;

		fread( &buffer, 1, sizeof( short ), file );

		if ( buffer != IMM_HEADER )
		{
			Warning( "[Improved Mute Manager] Invalid header of file \"muted_players.bin\"\n" );
			return;
		}

		buffer = 0;
		fread( &buffer, 1, sizeof( char ), file );

		if ( buffer < 1 )
		{
			Warning( "[Improved Mute Manager] Invalid version of file \"muted_players.bin\"\n" );
			return;
		}

		static struct MutedPlayerEntry
		{
			uint32 steamid_high;
			uint32 steamid_low;
			uint32 flags;
		} s_MutedPlayerBuffer;

		while ( fread( &s_MutedPlayerBuffer, 1, sizeof( MutedPlayerEntry ), file ) == sizeof( MutedPlayerEntry ) )
		{
			uint64 steamid = *reinterpret_cast<uint64_t *>( &s_MutedPlayerBuffer.steamid_high );

			g_MutedPlayers.Insert( steamid, s_MutedPlayerBuffer.flags );
		}

		fclose( file );
	}
	else
	{
		Warning( "[Improved Mute Manager] Missing file \"muted_players.bin\"\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: save hash table in file muted_players.bim
//-----------------------------------------------------------------------------

void SaveMutedPlayers()
{
	FILE *file = fopen( "muted_players.bin", "wb" );

	if ( file )
	{
		int buffer = 0;
		int saved_players = 0;

		buffer = IMM_HEADER;
		fwrite( &buffer, 1, sizeof( short ), file );

		buffer = IMM_VERSION;
		fwrite( &buffer, 1, sizeof( char ), file );

		for ( int i = 0; i < g_MutedPlayers.Count(); i++ )
		{
			HashTableIterator_t it = g_MutedPlayers.First( i );

			while ( g_MutedPlayers.IsValidIterator( it ) )
			{
				uint64 &steamid = g_MutedPlayers.KeyAt( i, it );
				uint32 &mute_flags = g_MutedPlayers.ValueAt( i, it );

				fwrite( &steamid, 1, sizeof( uint64 ), file );
				fwrite( &mute_flags, 1, sizeof( uint32 ), file );

				it = g_MutedPlayers.Next( i, it );
				++saved_players;
			}
		}

		Msg( "[Improved Mute Manager] Saved %d players in file \"muted_players.bin\"\n", saved_players );

		fclose( file );
	}
	else
	{
		Warning( "[Improved Mute Manager] Cannot create file \"muted_players.bin\"\n" );
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

FORCEINLINE uint32 *GetMutedPlayer( uint64 steamid )
{
	return g_MutedPlayers.Find( steamid );
}

//-----------------------------------------------------------------------------
// Purpose: add player in hash table or merge flags if player already in table
//-----------------------------------------------------------------------------

static void OnPlayerFound( uint32 *pFoundValue, uint32 *pInsertValue )
{
	*pFoundValue |= *pInsertValue;
}

FORCEINLINE bool AddMutedPlayer( uint64 steamid, uint32 flags )
{
	return g_MutedPlayers.Insert( steamid, flags, OnPlayerFound );
}

//-----------------------------------------------------------------------------
// Purpose: remove player from hash table if resulting flag is MUTE_NONE
//-----------------------------------------------------------------------------

static bool OnPlayerRemove( uint32 *pRemoveValue, uint32 *pUserValue )
{
	*pRemoveValue &= ~( *pUserValue );

	if ( *pRemoveValue != MUTE_NONE )
		return false;

	return true;
}

FORCEINLINE bool RemoveMutedPlayer( uint64 steamid, uint32 flags )
{
	return g_MutedPlayers.Remove( steamid, OnPlayerRemove, &flags );
}

//-----------------------------------------------------------------------------
// Console commands
//-----------------------------------------------------------------------------

#define IMM_CHECK_PLAYER(idx) \
	idx > 0 && idx <= MAXCLIENTS && \
	SvenModAPI()->GetClientState() == CLS_ACTIVE && \
	SvenModAPI()->EngineFuncs()->GetEntityByIndex(idx) && \
	SvenModAPI()->EngineFuncs()->GetEntityByIndex(idx) != SvenModAPI()->EngineFuncs()->GetLocalPlayer()

CON_COMMAND( imm_mute_voice, "Mute player's voice communication" )
{
	if ( g_bPaused )
		return;

	if ( args.ArgC() > 1 )
	{
		int nPlayerIndex = atoi( args[ 1 ] );

		if ( IMM_CHECK_PLAYER( nPlayerIndex ) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

			AddMutedPlayer( steamid, MUTE_VOICE );

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo( nPlayerIndex - 1 );

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg( "[Improved Mute Manager] Player \"%s\" muted (voice)\n", pPlayerInfo->name );
		}
	}
	else
	{
		ConMsg( "Usage:  imm_mute_voice <player index>\n" );
	}
}

CON_COMMAND( imm_mute_chat, "Mute player's chat communication" )
{
	if ( g_bPaused )
		return;

	if ( args.ArgC() > 1 )
	{
		int nPlayerIndex = atoi( args[ 1 ] );

		if ( IMM_CHECK_PLAYER( nPlayerIndex ) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

			AddMutedPlayer( steamid, MUTE_CHAT );

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo( nPlayerIndex - 1 );

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg( "[Improved Mute Manager] Player \"%s\" muted (chat)\n", pPlayerInfo->name );
		}
	}
	else
	{
		ConMsg( "Usage:  imm_mute_chat <player index>\n" );
	}
}

CON_COMMAND( imm_mute_all, "Mute all player communications" )
{
	if ( g_bPaused )
		return;

	if ( args.ArgC() > 1 )
	{
		int nPlayerIndex = atoi( args[ 1 ] );

		if ( IMM_CHECK_PLAYER( nPlayerIndex ) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

			AddMutedPlayer( steamid, MUTE_ALL );

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo( nPlayerIndex - 1 );

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg( "[Improved Mute Manager] Player \"%s\" muted\n", pPlayerInfo->name );
		}
	}
	else
	{
		ConMsg( "Usage:  imm_mute_all <player index>\n" );
	}
}

CON_COMMAND( imm_unmute_voice, "Unmute player's voice communication" )
{
	if ( g_bPaused )
		return;

	if ( args.ArgC() > 1 )
	{
		int nPlayerIndex = atoi( args[ 1 ] );

		if ( IMM_CHECK_PLAYER( nPlayerIndex ) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

			RemoveMutedPlayer( steamid, MUTE_VOICE );

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo( nPlayerIndex - 1 );

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg( "[Improved Mute Manager] Player \"%s\" unmuted (voice)\n", pPlayerInfo->name );
		}
	}
	else
	{
		ConMsg( "Usage:  imm_unmute_voice <player index>\n" );
	}
}

CON_COMMAND( imm_unmute_chat, "Unmute player's chat communication" )
{
	if ( g_bPaused )
		return;

	if ( args.ArgC() > 1 )
	{
		int nPlayerIndex = atoi( args[ 1 ] );

		if ( IMM_CHECK_PLAYER( nPlayerIndex ) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

			RemoveMutedPlayer( steamid, MUTE_CHAT );

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo( nPlayerIndex - 1 );

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg( "[Improved Mute Manager] Player \"%s\" unmuted (chat)\n", pPlayerInfo->name );
		}
	}
	else
	{
		ConMsg( "Usage:  imm_unmute_chat <player index>\n" );
	}
}

CON_COMMAND( imm_unmute_all, "Unmute all player communications" )
{
	if ( g_bPaused )
		return;

	if ( args.ArgC() > 1 )
	{
		int nPlayerIndex = atoi( args[ 1 ] );

		if ( IMM_CHECK_PLAYER( nPlayerIndex ) )
		{
			uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

			RemoveMutedPlayer( steamid, MUTE_ALL );

			player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo( nPlayerIndex - 1 );

			if ( pPlayerInfo && *pPlayerInfo->name )
				Msg( "[Improved Mute Manager] Player \"%s\" unmuted\n", pPlayerInfo->name );
		}
	}
	else
	{
		ConMsg( "Usage:  imm_unmute_all <player index>\n" );
	}
}

CON_COMMAND( imm_unmute_by_steamid64, "Unmute all player communications with given Steam64 ID" )
{
	if ( g_bPaused )
		return;

	if ( args.ArgC() > 1 )
	{
		uint64 steamid = atoll( args[ 1 ] );

		if ( RemoveMutedPlayer( steamid, MUTE_ALL ) )
		{
			Msg( "[Improved Mute Manager] SteamID %llu has been removed\n", steamid );
		}
		else
		{
			Msg( "[Improved Mute Manager] SteamID %llu not found\n", steamid );
		}
	}
	else
	{
		ConMsg( "Usage:  imm_unmute_by_steamid64 <Steam64 ID>\n" );
	}
}

CON_COMMAND( imm_save_to_file, "Save all muted players to file \"muted_players.bin\"" )
{
	if ( g_bPaused )
		return;

	SaveMutedPlayers();
}

CON_COMMAND( imm_print_muted_players, "Print all muted players" )
{
	if ( g_bPaused )
		return;

	Msg( "====================== Muted Players ======================\n" );

	int current_players = 1;

	for ( int i = 0; i < g_MutedPlayers.Count(); i++ )
	{
		HashTableIterator_t it = g_MutedPlayers.First( i );

		while ( g_MutedPlayers.IsValidIterator( it ) )
		{
			uint64 &steamid = g_MutedPlayers.KeyAt( i, it );
			uint32 &mute_flags = g_MutedPlayers.ValueAt( i, it );

			Msg( "%d >> SteamID: %llu | Voice: %d | Chat: %d\n", current_players, steamid, ( mute_flags & MUTE_VOICE ) != 0, ( mute_flags & MUTE_CHAT ) != 0 );

			it = g_MutedPlayers.Next( i, it );
			++current_players;
		}
	}

	Msg( "====================== Muted Players ======================\n" );
}

CON_COMMAND( imm_print_current_muted_players, "Print currently muted players" )
{
	if ( g_bPaused )
		return;

	if ( SvenModAPI()->GetClientState() != CLS_ACTIVE )
		return;

	Msg( "====================== Muted Players ======================\n" );

	cl_entity_t *pLocal = SvenModAPI()->EngineFuncs()->GetLocalPlayer();
	int nLocalPlayer = pLocal->index;

	for ( int i = 1; i <= MAXCLIENTS; ++i )
	{
		if ( i == nLocalPlayer )
			continue;

		uint64 steamid = PlayerUtils()->GetSteamID( i );

		if ( !steamid )
			continue;

		uint32 *mute_flags = GetMutedPlayer( steamid );

		if ( !mute_flags )
			continue;

		player_info_t *pPlayerInfo = SvenModAPI()->EngineStudio()->PlayerInfo( i - 1 );

		Msg( "#%d >> Player: \"%s\" | Voice: %d | Chat: %d\n", i, pPlayerInfo->name, ( *mute_flags & MUTE_VOICE ) != 0, ( *mute_flags & MUTE_CHAT ) != 0 );
	}

	Msg( "====================== Muted Players ======================\n" );
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

DECLARE_CLASS_FUNC( void, HOOKED_CVoiceBanMgr__SetPlayerBan, void *thisptr, char *pszPlayerUniqueID, bool bMute )
{
	uint64 steamid = PlayerUtils()->GetSteamID( g_nLastIndexedPlayer );

	if ( !steamid )
		return;

	if ( bMute )
		AddMutedPlayer( steamid, MUTE_VOICE );
	else
		RemoveMutedPlayer( steamid, MUTE_VOICE );
}

// Get pointer to muted player (won't hook CVoiceBanMgr::GetPlayerBan to save perfomance speed)

DECLARE_CLASS_FUNC( void *, HOOKED_CVoiceBanMgr__InternalFindPlayerSquelch, void *thisptr, char *pszPlayerUniqueID )
{
	uint64 steamid = PlayerUtils()->GetSteamID( g_nLastIndexedPlayer );

	if ( !steamid )
		return NULL;

	uint32 *mute_flags = GetMutedPlayer( steamid );

	if ( mute_flags && ( *mute_flags & MUTE_VOICE || imm_mute_all_communications.GetBool() ) )
		return mute_flags;

	return NULL;
}

DECLARE_CLASS_FUNC( bool, HOOKED_CVoiceStatus__IsPlayerBlocked, void *thisptr, int nPlayerIndex )
{
	uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

	if ( !steamid )
		return false;

	uint32 *mute_flags = GetMutedPlayer( steamid );

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

DECLARE_CLASS_FUNC( void, HOOKED_CVoiceStatus__SetPlayerBlockedState, void *thisptr, int nPlayerIndex, bool bMute )
{
	uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

	if ( !steamid )
		return;

	if ( bMute )
		AddMutedPlayer( steamid, MUTE_VOICE );
	else
		RemoveMutedPlayer( steamid, MUTE_VOICE );
}

// Send to server the mask of muted players that we don't want to hear

DECLARE_CLASS_FUNC( void, HOOKED_CVoiceStatus__UpdateServerState, void *thisptr, bool bForce )
{
	//if ( SvenModAPI()->GetClientState() != CLS_ACTIVE )
	//	return;

	static float flForceBanMaskTime = 0.f;
	static char command_buffer[ 128 ];

	char const *pLevelName = SvenModAPI()->EngineFuncs()->GetLevelName();
	bool bClientDebug = bool( voice_clientdebug->value );

	if ( *pLevelName == 0 && bClientDebug )
	{
		Msg( "CVoiceStatus::UpdateServerState: pLevelName[0]==0\n" );
		return;
	}

	uint32 banMask = 0;

	bool bMuteEverything = imm_mute_all_communications.GetBool();
	bool bVoiceModEnable = static_cast<bool>( voice_modenable->value );

	// thisptr members
	float *m_LastUpdateServerState = (float *)( (unsigned char *)thisptr + 0x18 );
	int *m_bServerModEnable = (int *)( (unsigned char *)thisptr + 0x1C );

	// validate cvar 'voice_modenable'
	if ( bForce || bool( *m_bServerModEnable ) != bVoiceModEnable )
	{
		*m_bServerModEnable = static_cast<int>( bVoiceModEnable );

		snprintf( command_buffer, sizeof( command_buffer ), "VModEnable %d", bVoiceModEnable );
		SvenModAPI()->EngineFuncs()->ClientCmd( command_buffer );

		command_buffer[ sizeof( command_buffer ) - 1 ] = 0;

		if ( bClientDebug )
			Msg( "CVoiceStatus::UpdateServerState: Sending '%s'\n", command_buffer );
	}

	// build ban mask
	for ( uint32 i = 1; i <= MAXCLIENTS; ++i )
	{
		uint64 steamid = PlayerUtils()->GetSteamID( i );

		if ( !steamid )
			continue;

		uint32 *mute_flags = GetMutedPlayer( steamid );

		if ( mute_flags && ( bMuteEverything || *mute_flags & MUTE_VOICE ) )
			banMask |= 1 << ( i - 1 ); // one bit, one client
	}

	if ( SvenModAPI()->EngineFuncs()->GetClientTime() - flForceBanMaskTime < 0.f )
	{
		flForceBanMaskTime = 0.f;
	}

	if ( g_BanMask != banMask || ( SvenModAPI()->EngineFuncs()->GetClientTime() - flForceBanMaskTime >= 5.0f ) )
	{
		snprintf( command_buffer, sizeof( command_buffer ), "vban %X", banMask ); // vban [ban_mask]

		if ( bClientDebug )
			Msg( "CVoiceStatus::UpdateServerState: Sending '%s'\n", command_buffer );

		SvenModAPI()->EngineFuncs()->ClientCmd( command_buffer );
		g_BanMask = banMask;
	}
	else if ( bClientDebug )
	{
		Msg( "CVoiceStatus::UpdateServerState: no change\n" );
	}

	*m_LastUpdateServerState = flForceBanMaskTime = SvenModAPI()->EngineFuncs()->GetClientTime();
}

DECLARE_FUNC( bool, __cdecl, HOOKED_HACK_GetPlayerUniqueID, int nPlayerIndex, char *pszPlayerUniqueID )
{
	g_nLastIndexedPlayer = nPlayerIndex;
	return ORIG_HACK_GetPlayerUniqueID( nPlayerIndex, pszPlayerUniqueID );
}

int UserMsgHook_SayText( const char *pszName, int iSize, void *pBuffer )
{
	SayTextBuffer.Init( pBuffer, iSize, true );
	SayTextBuffer.BeginReading();

	int result = 0;
	int nPlayerIndex = SayTextBuffer.ReadByte();

	uint64 steamid = PlayerUtils()->GetSteamID( nPlayerIndex );

	if ( !steamid )
	{
		g_bProcessingChat = true;

		result = ORIG_UserMsgHook_SayText( pszName, iSize, pBuffer );

		g_bProcessingChat = false;

		return result;
	}

	uint32 *mute_flags = GetMutedPlayer( steamid );

	if ( mute_flags )
	{
		if ( imm_mute_all_communications.GetBool() || *mute_flags & MUTE_CHAT )
			return 0;
	}

	g_bProcessingChat = true;

	result = ORIG_UserMsgHook_SayText( pszName, iSize, pBuffer );

	g_bProcessingChat = false;

	return result;
}

//-----------------------------------------------------------------------------
// CMuteManager interface
//-----------------------------------------------------------------------------

bool CMuteManager::MutePlayer( int index, int flags )
{
	uint64 steamid = PlayerUtils()->GetSteamID( index );

	if ( steamid )
	{
		return AddMutedPlayer( steamid, (uint32)flags );
	}

	return false;
}

bool CMuteManager::UnmutePlayer( int index, int flags )
{
	uint64 steamid = PlayerUtils()->GetSteamID( index );

	if ( steamid )
	{
		return RemoveMutedPlayer( steamid, (uint32)flags );
	}

	return false;
}

int CMuteManager::GetPlayerMuteFlags( int index )
{
	uint64 steamid = PlayerUtils()->GetSteamID( index );

	if ( steamid )
	{
		uint32 *pFlags = GetMutedPlayer( steamid );

		if ( pFlags != NULL )
			return (int)*pFlags;
	}

	return MUTE_NONE;
}

bool CMuteManager::IsInsideChat( void )
{
	return g_bProcessingChat;
}

void CMuteManager::SetInsideChat( bool state )
{
	g_bProcessingChat = state;
}

// Expose interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMuteManager, IMuteManager, MUTE_MANAGER_INTERFACE_VERSION, g_MuteManager );

//-----------------------------------------------------------------------------
// CMuteManager feature
//-----------------------------------------------------------------------------

bool CMuteManager::Load( void )
{
	/*
	m_pfnCHudBaseTextBlock__Print = MemoryUtils()->FindPattern( SvenModAPI()->Modules()->Client, CHudBaseTextBlock__Print );

	if ( !m_pfnCHudBaseTextBlock__Print )
	{
		Warning("[Improved Mute Manager] Can't find function CHudBaseTextBlock::Print\n");
		return false;
	}
	*/

	int patternIndex;
	bool ScanOK = true;

	DEFINE_PATTERNS_FUTURE( fCVoiceBanMgr__SetPlayerBan );
	DEFINE_PATTERNS_FUTURE( fCVoiceBanMgr__InternalFindPlayerSquelch );
	DEFINE_PATTERNS_FUTURE( fCVoiceStatus__IsPlayerBlocked );
	DEFINE_PATTERNS_FUTURE( fCVoiceStatus__SetPlayerBlockedState );
	DEFINE_PATTERNS_FUTURE( fCVoiceStatus__UpdateServerState );
	DEFINE_PATTERNS_FUTURE( fHACK_GetPlayerUniqueID );

	// Find signatures
	MemoryUtils()->FindPatternAsync( SvenModAPI()->Modules()->Client, Patterns::Client::CVoiceBanMgr__SetPlayerBan, fCVoiceBanMgr__SetPlayerBan );
	MemoryUtils()->FindPatternAsync( SvenModAPI()->Modules()->Client, Patterns::Client::CVoiceBanMgr__InternalFindPlayerSquelch, fCVoiceBanMgr__InternalFindPlayerSquelch );
	MemoryUtils()->FindPatternAsync( SvenModAPI()->Modules()->Client, Patterns::Client::CVoiceStatus__IsPlayerBlocked, fCVoiceStatus__IsPlayerBlocked );
	MemoryUtils()->FindPatternAsync( SvenModAPI()->Modules()->Client, Patterns::Client::CVoiceStatus__SetPlayerBlockedState, fCVoiceStatus__SetPlayerBlockedState );
	MemoryUtils()->FindPatternAsync( SvenModAPI()->Modules()->Client, Patterns::Client::CVoiceStatus__UpdateServerState, fCVoiceStatus__UpdateServerState );
	MemoryUtils()->FindPatternAsync( SvenModAPI()->Modules()->Client, Patterns::Client::HACK_GetPlayerUniqueID, fHACK_GetPlayerUniqueID );

	// CVoiceBanMgr::SetPlayerBan
	if ( ( m_pfnCVoiceBanMgr__SetPlayerBan = MemoryUtils()->GetPatternFutureValue( fCVoiceBanMgr__SetPlayerBan, &patternIndex ) ) == NULL )
	{
		Warning( "[IMM] Couldn't find function \"CVoiceBanMgr::SetPlayerBan\"\n" );
		ScanOK = false;
	}
	else
	{
		DevMsg( "[IMM] Found function \"CVoiceBanMgr::SetPlayerBan\" for version \"%s\"\n", GET_PATTERN_NAME_BY_INDEX( Patterns::Client::CVoiceBanMgr__SetPlayerBan, patternIndex ) );
	}
	
	// CVoiceBanMgr::InternalFindPlayerSquelch
	if ( ( m_pfnCVoiceBanMgr__InternalFindPlayerSquelch = MemoryUtils()->GetPatternFutureValue( fCVoiceBanMgr__InternalFindPlayerSquelch, &patternIndex ) ) == NULL )
	{
		Warning( "[IMM] Couldn't find function \"CVoiceBanMgr::InternalFindPlayerSquelch\"\n" );
		ScanOK = false;
	}
	else
	{
		DevMsg( "[IMM] Found function \"CVoiceBanMgr::InternalFindPlayerSquelch\" for version \"%s\"\n", GET_PATTERN_NAME_BY_INDEX( Patterns::Client::CVoiceBanMgr__InternalFindPlayerSquelch, patternIndex ) );
	}
	
	// CVoiceStatus::IsPlayerBlocked
	if ( ( m_pfnCVoiceStatus__IsPlayerBlocked = MemoryUtils()->GetPatternFutureValue( fCVoiceStatus__IsPlayerBlocked, &patternIndex ) ) == NULL )
	{
		Warning( "[IMM] Couldn't find function \"CVoiceStatus::IsPlayerBlocked\"\n" );
		ScanOK = false;
	}
	else
	{
		DevMsg( "[IMM] Found function \"CVoiceStatus::IsPlayerBlocked\" for version \"%s\"\n", GET_PATTERN_NAME_BY_INDEX( Patterns::Client::CVoiceStatus__IsPlayerBlocked, patternIndex ) );
	}
	
	// CVoiceStatus::SetPlayerBlockedState
	if ( ( m_pfnCVoiceStatus__SetPlayerBlockedState = MemoryUtils()->GetPatternFutureValue( fCVoiceStatus__SetPlayerBlockedState, &patternIndex ) ) == NULL )
	{
		Warning( "[IMM] Couldn't find function \"CVoiceStatus::SetPlayerBlockedState\"\n" );
		ScanOK = false;
	}
	else
	{
		DevMsg( "[IMM] Found function \"CVoiceStatus::SetPlayerBlockedState\" for version \"%s\"\n", GET_PATTERN_NAME_BY_INDEX( Patterns::Client::CVoiceStatus__SetPlayerBlockedState, patternIndex ) );
	}
	
	// CVoiceStatus::UpdateServerState
	if ( ( m_pfnCVoiceStatus__UpdateServerState = MemoryUtils()->GetPatternFutureValue( fCVoiceStatus__UpdateServerState, &patternIndex ) ) == NULL )
	{
		Warning( "[IMM] Couldn't find function \"CVoiceStatus::UpdateServerState\"\n" );
		ScanOK = false;
	}
	else
	{
		DevMsg( "[IMM] Found function \"CVoiceStatus::UpdateServerState\" for version \"%s\"\n", GET_PATTERN_NAME_BY_INDEX( Patterns::Client::CVoiceStatus__UpdateServerState, patternIndex ) );
	}
	
	// HACK_GetPlayerUniqueID
	if ( ( m_pfnHACK_GetPlayerUniqueID = MemoryUtils()->GetPatternFutureValue( fHACK_GetPlayerUniqueID, &patternIndex ) ) == NULL )
	{
		Warning( "[IMM] Couldn't find function \"HACK_GetPlayerUniqueID\"\n" );
		ScanOK = false;
	}
	else
	{
		DevMsg( "[IMM] Found function \"HACK_GetPlayerUniqueID\" for version \"%s\"\n", GET_PATTERN_NAME_BY_INDEX( Patterns::Client::HACK_GetPlayerUniqueID, patternIndex ) );
	}

	if ( !ScanOK )
		return false;

	// Get native cvars
	voice_clientdebug = SvenModAPI()->CVar()->FindCvar( "voice_clientdebug" );

	if ( !voice_clientdebug )
	{
		Warning( "[Improved Mute Manager] Can't find cvar \"voice_clientdebug\"\n" );
		return false;
	}

	voice_modenable = SvenModAPI()->CVar()->FindCvar( "voice_modenable" );

	if ( !voice_modenable )
	{
		Warning( "[Improved Mute Manager] Can't find cvar \"voice_modenable\"\n" );
		return false;
	}

	return true;
}

void CMuteManager::PostLoad( void )
{
	LoadMutedPlayers();
	ConVar_Register();

	//m_hCHudBaseTextBlock__Print = DetoursAPI()->DetourFunction( m_pfnCHudBaseTextBlock__Print, HOOKED_CHudBaseTextBlock__Print, GET_FUNC_PTR(ORIG_CHudBaseTextBlock__Print) );

	m_hCVoiceBanMgr__SetPlayerBan = DetoursAPI()->DetourFunction( m_pfnCVoiceBanMgr__SetPlayerBan, HOOKED_CVoiceBanMgr__SetPlayerBan, GET_FUNC_PTR( ORIG_CVoiceBanMgr__SetPlayerBan ) );
	m_hCVoiceBanMgr__InternalFindPlayerSquelch = DetoursAPI()->DetourFunction( m_pfnCVoiceBanMgr__InternalFindPlayerSquelch, HOOKED_CVoiceBanMgr__InternalFindPlayerSquelch, GET_FUNC_PTR( ORIG_CVoiceBanMgr__InternalFindPlayerSquelch ) );
	m_hCVoiceStatus__IsPlayerBlocked = DetoursAPI()->DetourFunction( m_pfnCVoiceStatus__IsPlayerBlocked, HOOKED_CVoiceStatus__IsPlayerBlocked, GET_FUNC_PTR( ORIG_CVoiceStatus__IsPlayerBlocked ) );
	m_hCVoiceStatus__SetPlayerBlockedState = DetoursAPI()->DetourFunction( m_pfnCVoiceStatus__SetPlayerBlockedState, HOOKED_CVoiceStatus__SetPlayerBlockedState, GET_FUNC_PTR( ORIG_CVoiceStatus__SetPlayerBlockedState ) );
	m_hCVoiceStatus__UpdateServerState = DetoursAPI()->DetourFunction( m_pfnCVoiceStatus__UpdateServerState, HOOKED_CVoiceStatus__UpdateServerState, GET_FUNC_PTR( ORIG_CVoiceStatus__UpdateServerState ) );
	m_hHACK_GetPlayerUniqueID = DetoursAPI()->DetourFunction( m_pfnHACK_GetPlayerUniqueID, HOOKED_HACK_GetPlayerUniqueID, GET_FUNC_PTR( ORIG_HACK_GetPlayerUniqueID ) );

	m_hUserMsgHook_SayText = Hooks()->HookUserMessage( "SayText", UserMsgHook_SayText, &ORIG_UserMsgHook_SayText );
}

void CMuteManager::Unload( void )
{
	if ( imm_autosave_to_file.GetBool() )
		SaveMutedPlayers();

	RemoveMutedPlayers();

	//DetoursAPI()->RemoveDetour( m_hCHudBaseTextBlock__Print );
	DetoursAPI()->RemoveDetour( m_hCVoiceBanMgr__SetPlayerBan );
	DetoursAPI()->RemoveDetour( m_hCVoiceBanMgr__InternalFindPlayerSquelch );
	DetoursAPI()->RemoveDetour( m_hCVoiceStatus__IsPlayerBlocked );
	DetoursAPI()->RemoveDetour( m_hCVoiceStatus__SetPlayerBlockedState );
	DetoursAPI()->RemoveDetour( m_hCVoiceStatus__UpdateServerState );
	DetoursAPI()->RemoveDetour( m_hHACK_GetPlayerUniqueID );

	Hooks()->UnhookUserMessage( m_hUserMsgHook_SayText );

	ConVar_Unregister();
}

void CMuteManager::Pause( void )
{
	g_bPaused = true;

	//DetoursAPI()->PauseDetour( m_hCHudBaseTextBlock__Print );
	DetoursAPI()->PauseDetour( m_hCVoiceBanMgr__SetPlayerBan );
	DetoursAPI()->PauseDetour( m_hCVoiceBanMgr__InternalFindPlayerSquelch );
	DetoursAPI()->PauseDetour( m_hCVoiceStatus__IsPlayerBlocked );
	DetoursAPI()->PauseDetour( m_hCVoiceStatus__SetPlayerBlockedState );
	DetoursAPI()->PauseDetour( m_hCVoiceStatus__UpdateServerState );
	DetoursAPI()->PauseDetour( m_hHACK_GetPlayerUniqueID );

	Hooks()->UnhookUserMessage( m_hUserMsgHook_SayText );
}

void CMuteManager::Unpause( void )
{
	g_bPaused = false;

	//DetoursAPI()->UnpauseDetour( m_hCHudBaseTextBlock__Print );
	DetoursAPI()->UnpauseDetour( m_hCVoiceBanMgr__SetPlayerBan );
	DetoursAPI()->UnpauseDetour( m_hCVoiceBanMgr__InternalFindPlayerSquelch );
	DetoursAPI()->UnpauseDetour( m_hCVoiceStatus__IsPlayerBlocked );
	DetoursAPI()->UnpauseDetour( m_hCVoiceStatus__SetPlayerBlockedState );
	DetoursAPI()->UnpauseDetour( m_hCVoiceStatus__UpdateServerState );
	DetoursAPI()->UnpauseDetour( m_hHACK_GetPlayerUniqueID );

	m_hUserMsgHook_SayText = Hooks()->HookUserMessage( "SayText", UserMsgHook_SayText, &ORIG_UserMsgHook_SayText );
}