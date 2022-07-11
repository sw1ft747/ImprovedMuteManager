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
		EXTERN_PATTERN(CHudBaseTextBlock__Print);

		EXTERN_PATTERN(CVoiceBanMgr__SetPlayerBan);
		EXTERN_PATTERN(CVoiceBanMgr__InternalFindPlayerSquelch);

		EXTERN_PATTERN(CVoiceStatus__IsPlayerBlocked);
		EXTERN_PATTERN(CVoiceStatus__SetPlayerBlockedState);
		EXTERN_PATTERN(CVoiceStatus__UpdateServerState);

		EXTERN_PATTERN(HACK_GetPlayerUniqueID);
	}

	void ResolvePatterns( void );
}

#endif