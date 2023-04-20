#ifndef IMUTEMANAGER_H
#define IMUTEMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include <platform.h>

// Mute flags
#define MUTE_NONE ( 0 )
#define MUTE_VOICE ( 0x10 )
#define MUTE_CHAT ( 0x20 )
#define MUTE_ALL ( MUTE_VOICE | MUTE_CHAT )

//-----------------------------------------------------------------------------
// Purpose: Improved Mute Manager control interface
//-----------------------------------------------------------------------------

abstract_class IMuteManager
{
public:
	virtual			~IMuteManager() {}

	virtual bool	MutePlayer( int index, int flags ) = 0;
	virtual bool	UnmutePlayer( int index, int flags ) = 0;

	virtual int		GetPlayerMuteFlags( int index ) = 0;

	virtual bool	IsInsideChat( void ) = 0;
	virtual void	SetInsideChat( bool state ) = 0;
};

extern IMuteManager *g_pMuteManager;

#define MUTE_MANAGER_INTERFACE_VERSION "MuteManager001"

#endif