// C++
// Patterns

#pragma once

#include "utils/patterns_base.h"

namespace Patterns
{
	namespace Interfaces
	{
		INIT_PATTERN(EngineFuncs);
		INIT_PATTERN(ClientFuncs);
		INIT_PATTERN(EngineStudio);
	}

	namespace Client
	{
		INIT_PATTERN(CHudBaseTextBlock__Print);

		INIT_PATTERN(CVoiceBanMgr__SaveState);

		INIT_PATTERN(CVoiceBanMgr__SetPlayerBan);
		INIT_PATTERN(CVoiceBanMgr__InternalFindPlayerSquelch);

		INIT_PATTERN(CVoiceStatus__IsPlayerBlocked);
		INIT_PATTERN(CVoiceStatus__SetPlayerBlockedState);

		INIT_PATTERN(CVoiceStatus__UpdateServerState);

		INIT_PATTERN(HACK_GetPlayerUniqueID);
	}
}