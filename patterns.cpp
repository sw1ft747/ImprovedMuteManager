#include <ISvenModAPI.h>
#include <dbg.h>

#include "patterns.h"

namespace Patterns
{
	namespace Client
	{
		DEFINE_PATTERNS_1( CHudBaseTextBlock__Print,
						   "5.25",
						   "55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 53 56 57 A1 ? ? ? ? 33 C5 50 8D 45 F4 64 A3 00 00 00 00 8B D9 8B 0D" );

		DEFINE_PATTERNS_1( CVoiceBanMgr__SetPlayerBan,
						   "5.25",
						   "56 FF 74 24 08 8B F1 E8 ? ? ? ? 80 7C 24 0C 00 74 13 85 C0 75 32 FF 74 24 08 8B CE E8" );

		DEFINE_PATTERNS_1( CVoiceBanMgr__InternalFindPlayerSquelch,
						   "5.25",
						   "53 55 8B 6C 24 0C 56 57 0F 10 4D 00 0F 28 C1 66 0F 73 D8 08 66 0F FC C8 0F 10 C1 66 0F 73 D8 04" );

		DEFINE_PATTERNS_1( CVoiceStatus__IsPlayerBlocked,
						   "5.25",
						   "83 EC 14 A1 ? ? ? ? 33 C4 89 44 24 10 56 8D 44 24 04 8B F1 50 FF 74 24 20 FF 15" );

		DEFINE_PATTERNS_1( CVoiceStatus__SetPlayerBlockedState,
						   "5.25",
						   "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 68 ? ? ? ? 8B D9 FF 15 ? ? ? ? D9 5C 24 08" );

		DEFINE_PATTERNS_1( CVoiceStatus__UpdateServerState,
						   "5.25",
						   "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 8B D9 89 5C 24 08" );

		DEFINE_PATTERNS_1( HACK_GetPlayerUniqueID,
						   "5.25",
						   "FF 74 24 08 FF 74 24 08 FF 15 ? ? ? ? 83 C4 08 85 C0 0F 95 C0 C3" );
	}
}