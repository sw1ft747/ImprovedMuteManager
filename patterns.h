#ifndef PATTERNS_H
#define PATTERNS_H

#ifdef _WIN32
#pragma once
#endif

#include <memutils/patterns.h>

namespace Patterns
{
	namespace Client
	{
		EXTERN_PATTERNS(CHudBaseTextBlock__Print);

		EXTERN_PATTERNS(CVoiceBanMgr__SetPlayerBan);
		EXTERN_PATTERNS(CVoiceBanMgr__InternalFindPlayerSquelch);

		EXTERN_PATTERNS(CVoiceStatus__IsPlayerBlocked);
		EXTERN_PATTERNS(CVoiceStatus__SetPlayerBlockedState);
		EXTERN_PATTERNS(CVoiceStatus__UpdateServerState);

		EXTERN_PATTERNS(HACK_GetPlayerUniqueID);
	}
}

#endif