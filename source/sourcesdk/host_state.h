//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HOST_STATE_H
#define HOST_STATE_H

#include "mathlib/vector.h"

typedef enum 
{
	HS_NEW_GAME = 0,
	HS_LOAD_GAME,
	HS_CHANGE_LEVEL_SP,
	HS_CHANGE_LEVEL_MP,
	HS_RUN,
	HS_GAME_SHUTDOWN,
	HS_SHUTDOWN,
	HS_RESTART,
} HOSTSTATES;

class CHostState
{
public:
				CHostState();
	void		Init();
	void		FrameUpdate( float time );
	void		SetNextState( HOSTSTATES nextState );

	void		RunGameInit();

	void		SetState( HOSTSTATES newState, bool clearNext );
	void		GameShutdown();

	void		State_NewGame();
	void		State_LoadGame();
	void		State_ChangeLevelMP();
	void		State_ChangeLevelSP();
	void		State_Run( float time );
	void		State_GameShutdown();
	void		State_Shutdown();
	void		State_Restart();

	bool		IsGameShuttingDown();

	void		RememberLocation();
	void		OnClientConnected(); // Client fully connected
	void		OnClientDisconnected(); // Client disconnected

	HOSTSTATES	m_currentState;
	HOSTSTATES	m_nextState;
	Vector		m_vecLocation;
	QAngle		m_angLocation;
	char		m_levelName[256];
	char		m_landmarkName[256];
	char		m_saveName[256];
	float		m_flShortFrameTime;		// run a few one-tick frames to avoid large timesteps while loading assets

	bool		m_activeGame;
	bool		m_bRememberLocation;
	bool		m_bBackgroundLevel;
	bool		m_bWaitingForConnection;
};

#endif // HOST_STATE_H