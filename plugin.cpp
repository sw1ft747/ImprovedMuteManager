#include <IClientPlugin.h>
#include <interface.h>

#include <base_feature.h>
#include <convar.h>
#include <dbg.h>

#include "patterns.h"

//-----------------------------------------------------------------------------
// Mute manager's plugin interface
//-----------------------------------------------------------------------------

class CMuteManagerPlugin : public IClientPlugin
{
public:
	virtual api_version_t GetAPIVersion();

	virtual bool Load( CreateInterfaceFn pfnSvenModFactory, ISvenModAPI *pSvenModAPI, IPluginHelpers *pPluginHelpers );

	virtual void PostLoad( bool bGlobalLoad );

	virtual void Unload( void );

	virtual bool Pause( void );

	virtual void Unpause( void );

	virtual void OnFirstClientdataReceived( client_data_t *pcldata, float flTime );

	virtual void OnBeginLoading( void );

	virtual void OnEndLoading( void );

	virtual void OnDisconnect( void );

	virtual void GameFrame( client_state_t state, double frametime, bool bPostRunCmd );

	virtual void Draw( void );

	virtual void DrawHUD( float time, int intermission );

	virtual const char *GetName( void );

	virtual const char *GetAuthor( void );

	virtual const char *GetVersion( void );

	virtual const char *GetDescription( void );

	virtual const char *GetURL( void );

	virtual const char *GetDate( void );

	virtual const char *GetLogTag( void );
};

//-----------------------------------------------------------------------------
// Plugin's implementation
//-----------------------------------------------------------------------------

api_version_t CMuteManagerPlugin::GetAPIVersion()
{
	return SVENMOD_API_VER;
}

bool CMuteManagerPlugin::Load( CreateInterfaceFn pfnSvenModFactory, ISvenModAPI *pSvenModAPI, IPluginHelpers *pPluginHelpers )
{
	if ( !LoadFeatures() )
	{
		Warning( "[Improved Mute Manager] Failed to initialize\n" );
		return false;
	}

	ConColorMsg( { 40, 255, 40, 255 }, "[Improved Mute Manager] Successfully loaded\n" );
	return true;
}

void CMuteManagerPlugin::PostLoad( bool bGlobalLoad )
{
	PostLoadFeatures();
}

void CMuteManagerPlugin::Unload( void )
{
	UnloadFeatures();
}

bool CMuteManagerPlugin::Pause( void )
{
	PauseFeatures();
	return true;
}

void CMuteManagerPlugin::Unpause( void )
{
	UnpauseFeatures();
}

void CMuteManagerPlugin::OnFirstClientdataReceived( client_data_t *pcldata, float flTime )
{
}

void CMuteManagerPlugin::OnBeginLoading( void )
{
}

void CMuteManagerPlugin::OnEndLoading( void )
{
}

void CMuteManagerPlugin::OnDisconnect( void )
{
}

void CMuteManagerPlugin::GameFrame( client_state_t state, double frametime, bool bPostRunCmd )
{
}

void CMuteManagerPlugin::Draw( void )
{
}

void CMuteManagerPlugin::DrawHUD( float time, int intermission )
{
}

const char *CMuteManagerPlugin::GetName( void )
{
	return "Improved Mute Manager";
}

const char *CMuteManagerPlugin::GetAuthor( void )
{
	return "Sw1ft";
}

const char *CMuteManagerPlugin::GetVersion( void )
{
	return "2.1.0";
}

const char *CMuteManagerPlugin::GetDescription( void )
{
	return "Improves Sven Co-op's mute manager";
}

const char *CMuteManagerPlugin::GetURL( void )
{
	return "https://github.com/sw1ft747/ImprovedMuteManager";
}

const char *CMuteManagerPlugin::GetDate( void )
{
	return SVENMOD_BUILD_TIMESTAMP;
}

const char *CMuteManagerPlugin::GetLogTag( void )
{
	return "IMM";
}

//-----------------------------------------------------------------------------
// Export the interface
//-----------------------------------------------------------------------------

EXPOSE_SINGLE_INTERFACE( CMuteManagerPlugin, IClientPlugin, CLIENT_PLUGIN_INTERFACE_VERSION );