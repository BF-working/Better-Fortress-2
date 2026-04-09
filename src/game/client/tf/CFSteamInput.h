/** \file CFSteamInput.cpp
 * Main definition file for wrapping Steam Input API functions for Custom Fortress 2.
 */

#ifndef CF_STEAMINPUT_H
#define CF_STEAMINPUT_H
#pragma once

#include <igamesystem.h>
#include <steam/steam_api.h>

/**
 * \brief Types of Action Sets for Custom Fortress 2.
 * \warning This must be in-sync with Custom Fortress 2's IGA file.
 */
typedef enum CFInputActionSet_e {
	ACTIONSET_NONE = 0,

	ACTIONSET_MAINMENU,
	ACTIONSET_INGAME,

	ACTIONSET_COUNT
} CFInputActionSet_t;

/**
 * \brief Types of controller actions for Custom Fortress 2.
 * \warning This must be in-sync with Custom Fortress 2's IGA file. (customfortress/cfg/steam_input/action_manifest.vdf)
 */
typedef enum CFInputActions_e {
	ACTIONS_NONE = 0,

	ACTION_PRIMARYATTACK,
	ACTION_SECONDARYATTACK,
	ACTION_TAUNT,
	ACTION_CHANGETEAM,
	ACTION_CHANGECLASS,
	ACTION_CROUCH,
	ACTION_JUMP,
	ACTION_USE,
	ACTION_ESCAPE,

	ACTIONS_COUNT
} CFInputActions_t;

/*! \class CCFSteamInputManager
 * \brief Steam Input implementation for Custom Fortress 2.
 */
class CCFSteamInputManager : public CAutoGameSystemPerFrame {
public:
	CCFSteamInputManager();
	~CCFSteamInputManager() override;

	bool				Init() override;
	void				Shutdown() override;

	void				Update(float frametime) override;

	void				LevelInitPreEntity() override;
	void				LevelInitPostEntity() override;
	void				LevelShutdownPreEntity() override;
	void				LevelShutdownPostEntity() override;

	void				SetActionSet(CFInputActionSet_t iSet);
	void				ShowBindingPanel(int iControllerIndex);

	int					GetConnectedGamepadsCount() const { return gamepadsCount_; }
	bool				IsControllerConnected(int iControllerIndex) const;

	virtual void		OnCommand(const char* pszCommand);

protected:
	virtual void		UpdateActionStates();

	STEAM_CALLBACK(CCFSteamInputManager, OnOSKHidden, GamepadTextInputDismissed_t, onOskHidden_);

private:
	bool					initialized_;
	CFInputActionSet_t		currentSet_;
	InputHandle_t*			gamepads_;
	int						gamepadsCount_;

	CUtlMap<InputDigitalActionHandle_t, bool> digitalActions_;

	ISteamInput*			input_;
};

CCFSteamInputManager* SteamInputManager();

#endif // !CF_STEAMINPUT_H
