// C++
// Patterns

#include "patterns.h"

namespace Patterns
{
	namespace Interfaces
	{
		PATTERN(EngineFuncs, "68 ? ? ? ? FF 15 ? ? ? ? A1 ? ? ? ? 83 C4 1C 85 C0 74 0A 68");
		PATTERN(ClientFuncs, "FF E0 68 ? ? ? ? FF 35 ? ? ? ? E8 ? ? ? ? 83 C4 08 A3");
		PATTERN(EngineStudio, "68 ? ? ? ? 68 ? ? ? ? 6A 01 FF D0 83 C4 0C 85 C0");
	}

	namespace Client
	{
		PATTERN(CHudBaseTextBlock__Print, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 53 56 57 A1 ? ? ? ? 33 C5 50 8D 45 F4 64 A3 00 00 00 00 8B D9 8B 0D");

		PATTERN(CVoiceBanMgr__SaveState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 8B 84 24 ? ? 00 00 53 57 FF 35 ? ? ? ? 8B F9 50 8D 44 24 14");

		PATTERN(CVoiceBanMgr__SetPlayerBan, "56 FF 74 24 08 8B F1 E8 ? ? ? ? 80 7C 24 0C 00 74 13 85 C0 75 32 FF 74 24 08 8B CE E8");
		PATTERN(CVoiceBanMgr__InternalFindPlayerSquelch, "53 55 8B 6C 24 0C 56 57 0F 10 4D 00 0F 28 C1 66 0F 73 D8 08 66 0F FC C8 0F 10 C1 66 0F 73 D8 04");

		PATTERN(CVoiceStatus__IsPlayerBlocked, "83 EC 14 A1 ? ? ? ? 33 C4 89 44 24 10 56 8D 44 24 04 8B F1 50 FF 74 24 20 FF 15");
		PATTERN(CVoiceStatus__SetPlayerBlockedState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 68 ? ? ? ? 8B D9 FF 15 ? ? ? ? D9 5C 24 08");

		PATTERN(CVoiceStatus__UpdateServerState, "81 EC ? ? 00 00 A1 ? ? ? ? 33 C4 89 84 24 ? ? 00 00 53 8B D9 89 5C 24 08");

		PATTERN(HACK_GetPlayerUniqueID, "FF 74 24 08 FF 74 24 08 FF 15 ? ? ? ? 83 C4 08 85 C0 0F 95 C0 C3");

		//PATTERN(CVoiceStatus__UpdateBanButton, "83 EC 20 A1 ? ? ? ? 33 C4 89 44 24 1C 53 55 8B 6C 24 2C 8B D9 57 8B BC AB ? ? ? ? 85 FF");
	}
}