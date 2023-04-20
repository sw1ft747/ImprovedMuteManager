#ifndef MUTEMANAGER_H
#define MUTEMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "public/IMuteManager.h"

#include <base_feature.h>
#include <IDetoursAPI.h>

//-----------------------------------------------------------------------------
// Improved Mute Manager
//-----------------------------------------------------------------------------

class CMuteManager : public CBaseFeature, IMuteManager
{
public:
	// IMuteManager interface
	virtual bool MutePlayer( int index, int flags ) override;
	virtual bool UnmutePlayer( int index, int flags ) override;

	virtual int GetPlayerMuteFlags( int index ) override;

	virtual bool IsInsideChat( void ) override;
	virtual void SetInsideChat( bool state ) override;

	// CBaseFeature abstract class
	virtual bool Load( void ) override;

	virtual void PostLoad( void ) override;

	virtual void Unload( void ) override;

	virtual void Pause( void ) override;
	virtual void Unpause( void ) override;

private:
	//void *m_pfnCHudBaseTextBlock__Print;
	void *m_pfnCVoiceBanMgr__SetPlayerBan;
	void *m_pfnCVoiceBanMgr__InternalFindPlayerSquelch;
	void *m_pfnCVoiceStatus__IsPlayerBlocked;
	void *m_pfnCVoiceStatus__SetPlayerBlockedState;
	void *m_pfnCVoiceStatus__UpdateServerState;
	void *m_pfnHACK_GetPlayerUniqueID;

	//DetourHandle_t m_hCHudBaseTextBlock__Print;
	DetourHandle_t m_hCVoiceBanMgr__SetPlayerBan;
	DetourHandle_t m_hCVoiceBanMgr__InternalFindPlayerSquelch;
	DetourHandle_t m_hCVoiceStatus__IsPlayerBlocked;
	DetourHandle_t m_hCVoiceStatus__SetPlayerBlockedState;
	DetourHandle_t m_hCVoiceStatus__UpdateServerState;
	DetourHandle_t m_hHACK_GetPlayerUniqueID;

	DetourHandle_t m_hUserMsgHook_SayText;
};

extern CMuteManager g_MuteManager;


#endif