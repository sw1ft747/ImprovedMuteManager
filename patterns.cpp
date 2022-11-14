#include <ISvenModAPI.h>
#include <dbg.h>

#include "patterns.h"

namespace Patterns
{
	namespace Client
	{
		DEFINE_NULL_PATTERN(CHudBaseTextBlock__Print);
		DEFINE_NULL_PATTERN(CVoiceBanMgr__SetPlayerBan);
		DEFINE_NULL_PATTERN(CVoiceBanMgr__InternalFindPlayerSquelch);
		DEFINE_NULL_PATTERN(CVoiceStatus__IsPlayerBlocked);
		DEFINE_NULL_PATTERN(CVoiceStatus__SetPlayerBlockedState);
		DEFINE_NULL_PATTERN(CVoiceStatus__UpdateServerState);
		DEFINE_NULL_PATTERN(HACK_GetPlayerUniqueID);

		DEFINE_PATTERN(CHudBaseTextBlock__Print_v525, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 53 56 57 A1 ? ? ? ? 33 C5 50 8D 45 F4 64 A3 00 00 00 00 8B D9 8B 0D");

		DEFINE_PATTERN(CVoiceBanMgr__SetPlayerBan_v525, "56 FF 74 24 08 8B F1 E8 ? ? ? ? 80 7C 24 0C 00 74 13 85 C0 75 32 FF 74 24 08 8B CE E8");

		DEFINE_PATTERN(CVoiceBanMgr__InternalFindPlayerSquelch_v525, "53 55 8B 6C 24 0C 56 57 0F 10 4D 00 0F 28 C1 66 0F 73 D8 08 66 0F FC C8 0F 10 C1 66 0F 73 D8 04");

		DEFINE_PATTERN(CVoiceStatus__IsPlayerBlocked_v525, "83 EC 14 A1 ? ? ? ? 33 C4 89 44 24 10 56 8D 44 24 04 8B F1 50 FF 74 24 20 FF 15");

		DEFINE_PATTERN(CVoiceStatus__SetPlayerBlockedState_v525, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 68 ? ? ? ? 8B D9 FF 15 ? ? ? ? D9 5C 24 08");

		DEFINE_PATTERN(CVoiceStatus__UpdateServerState_v525, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 8B D9 89 5C 24 08");

		DEFINE_PATTERN(HACK_GetPlayerUniqueID_v525, "FF 74 24 08 FF 74 24 08 FF 15 ? ? ? ? 83 C4 08 85 C0 0F 95 C0 C3");
	}

	void ResolvePatterns( void )
	{
		client_version_s *pClientVersion = SvenModAPI()->GetClientVersion();

		switch ( pClientVersion->version )
		{
		case 525:
			Client::CHudBaseTextBlock__Print = Client::CHudBaseTextBlock__Print_v525;
			Client::CVoiceBanMgr__SetPlayerBan = Client::CVoiceBanMgr__SetPlayerBan_v525;
			Client::CVoiceBanMgr__InternalFindPlayerSquelch = Client::CVoiceBanMgr__InternalFindPlayerSquelch_v525;
			Client::CVoiceStatus__IsPlayerBlocked = Client::CVoiceStatus__IsPlayerBlocked_v525;
			Client::CVoiceStatus__SetPlayerBlockedState = Client::CVoiceStatus__SetPlayerBlockedState_v525;
			Client::CVoiceStatus__UpdateServerState = Client::CVoiceStatus__UpdateServerState_v525;
			Client::HACK_GetPlayerUniqueID = Client::HACK_GetPlayerUniqueID_v525;
			break;

		default:
			Warning("[Improved Mute Manager] There're no signatures for the version of client \"%s\". Going to try use signatures for the latest version \"5.25\"\n", pClientVersion->string);

			Client::CHudBaseTextBlock__Print = Client::CHudBaseTextBlock__Print_v525;
			Client::CVoiceBanMgr__SetPlayerBan = Client::CVoiceBanMgr__SetPlayerBan_v525;
			Client::CVoiceBanMgr__InternalFindPlayerSquelch = Client::CVoiceBanMgr__InternalFindPlayerSquelch_v525;
			Client::CVoiceStatus__IsPlayerBlocked = Client::CVoiceStatus__IsPlayerBlocked_v525;
			Client::CVoiceStatus__SetPlayerBlockedState = Client::CVoiceStatus__SetPlayerBlockedState_v525;
			Client::CVoiceStatus__UpdateServerState = Client::CVoiceStatus__UpdateServerState_v525;
			Client::HACK_GetPlayerUniqueID = Client::HACK_GetPlayerUniqueID_v525;
			break;
		}
	}
}