/** \file CFSteamInput.cpp
 * Main implementation file for wrapping Steam Input API functions for Custom Fortress 2.
 */

#include "cbase.h"
#include "CFSteamInput.h"
#include <clientsteamcontext.h>
#include <steam/steam_api.h>
#include <input.h>

ConVar cf_steaminput_verbose(
	"cf_steaminput_verbose",
	"0",
	FCVAR_ARCHIVE | FCVAR_CHEAT,
	"Verbose level for the logger in Custom Fortress 2's Steam Input manager."
);

#define CFSTEAMINPUT_LOG_VERBOSE(verboseLevel, ...) \
	do {\
		if (cf_steaminput_verbose.GetInt() >= verboseLevel) \
			ConColorMsg(Color(0, 50, 255, 255), "[CF Steam Input]: " __VA_ARGS__); \
	} while(0)

#define CFSTEAMINPUT_LOG_INFO(...) \
	ConColorMsg(Color(0, 50, 255, 255), "[CF Steam Input]: " __VA_ARGS__)

#define CFSTEAMINPUT_LOG_WARN(...) \
	ConColorMsg(Color(0, 50, 255, 255), "[CF Steam Input]: " __VA_ARGS__)

#define CFSTEAMINPUT_LOG_ERROR(...) \
	ConColorMsg(Color(235, 50, 25, 255), "[CF Steam Input]: " __VA_ARGS__)

/*! \var const char* pszActionSetNames
 * \brief Strings for types of Action Sets for Custom Fortress 2.
 * \warning This must be in-sync with Custom Fortress 2's IGA file.
 */
const char* pszActionSetNames[ACTIONSET_COUNT] = {
	"",				// ACTIONSET_NONE

	"MainMenu",		// ACTIONSET_MAINMENU
	"InGame",		// ACTIONSET_INGAME
};

/*! \var const char* pszActionsNames
 * \brief Strings for types of controller actions for Custom Fortress 2.
 * \warning This must be in-sync with Custom Fortress 2's IGA file.
 */
const char* pszActionsNames[ACTIONS_COUNT] = {
	"",						// ACTIONS_NONE

	"PrimaryAttack",		// ACTIONS_PRIMARYATTACK
	"SecondaryAttack",		// ACTIONS_SECONDARYATTACK
	"Taunt",				// ACTIONS_TAUNT
	"ChangeTeam",			// ACTIONS_CHANGETEAM
	"Duck",					// ACTIONS_CROUCH
	"Jump",					// ACTIONS_JUMP
	"Use",					// ACTIONS_USE
	"Escape",				// ACTIONS_ESCAPE
};

/*! \fn CCFSteamInputManager::CCFSteamInputManager()
 * \brief Constructor.
 */
CCFSteamInputManager::CCFSteamInputManager()
	: CAutoGameSystemPerFrame("cf_steam_input")
	, onOskHidden_(this, &CCFSteamInputManager::OnOSKHidden)
	, initialized_(false)
	, currentSet_(ACTIONSET_NONE)
	, gamepads_(new InputHandle_t[STEAM_INPUT_MAX_COUNT])
	, gamepadsCount_(0)
	, input_(nullptr) {
}

/*! \fn CCFSteamInputManager::~CCFSteamInputManager()
 * \brief Destructor.
 */
CCFSteamInputManager::~CCFSteamInputManager() {
}

/*! \fn bool CCFSteamInputManager::Init
 * \brief Initialization.
 * \returns true on successful initialization, otherwise false.
 */
bool CCFSteamInputManager::Init() {
	input_ = steamapicontext->SteamInput();
	if (!input_) {
		CFSTEAMINPUT_LOG_WARN("Failed to get the Steam Input API interface!\n");
		return false;
	}

	if (!input_->Init(true)) {
		CFSTEAMINPUT_LOG_WARN("Failed to initialize Steam Input API!\n");
		return false;
	}

	// Call RunFrame because the comments for this function states that
	// this must be called before GetControlledControllers
	input_->RunFrame();

	gamepadsCount_ = input_->GetConnectedControllers(gamepads_);

	SetActionSet(ACTIONSET_MAINMENU);

	initialized_ = true;

	return true;
}

/*! \fn void CCFSteamInputManager::Shutdown()
 * \brief Destruction.
 */
void CCFSteamInputManager::Shutdown() {
	if (!input_->Shutdown()) {
		CFSTEAMINPUT_LOG_WARN("Failed to shut down Steam Input API for some odd reason.\n");
	}
}

/*! \fn void CCFSteamInputManager::Update(float frametime)
 * \brief Updates the callbacks.
 */
void CCFSteamInputManager::Update(float) {
	input_->RunFrame();

	UpdateActionStates();
}

/*! \fn void CCFSteamInputManager::UpdateActionStates()
 * \brief Updates each action's state that were stored internally.
 */
void CCFSteamInputManager::UpdateActionStates() {
	for (int i = 0; i < gamepadsCount_; i++) {
		InputHandle_t hGamepadHandle = gamepads_[i];

		for (int j = 0; j < ACTIONS_COUNT; j++) {
			const char* pszActionName = pszActionsNames[j];
			InputDigitalActionHandle_t hActionHandle = input_->GetDigitalActionHandle(pszActionName);
			InputDigitalActionData_t hActionData = input_->GetDigitalActionData(hGamepadHandle, hActionHandle);

			bool bNewState = hActionData.bState;
			bool bCurrentState = digitalActions_[hActionHandle];
			if (bCurrentState == bNewState) continue;
			CFSTEAMINPUT_LOG_VERBOSE(1, "%s: state - %d > %d\n", pszActionName, bCurrentState, bNewState);
			digitalActions_[hActionHandle] = bNewState;
		}
	}
}

/*! \fn void CCFSteamInputManager::LevelInitPreEntity()
 * \brief Called after the level is starting to initialize, before the entities were initialized.
 */
void CCFSteamInputManager::LevelInitPreEntity() {
}

/*! \fn void CCFSteamInputManager::LevelInitPostEntity()
 * \brief Called after the level has been fully initialized.
 */
void CCFSteamInputManager::LevelInitPostEntity() {
	SetActionSet(ACTIONSET_INGAME);
}

/*! \fn void CCFSteamInputManager::LevelShutdownPreEntity()
 * \brief Called after the level is starting to shut down, before the entities were initialized.
 */
void CCFSteamInputManager::LevelShutdownPreEntity() {
}

/*! \fn void CCFSteamInputManager::LevelShutdownPostEntity()
 * \brief Called after the level has been fully shutted down.
 */
void CCFSteamInputManager::LevelShutdownPostEntity() {
	SetActionSet(ACTIONSET_MAINMENU);
}

/*! \fn void CCFSteamInputManager::SetActionSet(CFInputActionSet_t iSet)
 * \brief Sets a new action set for all of the connected controllers.
 * \param iSet The new action set to change to.
 */
void CCFSteamInputManager::SetActionSet(CFInputActionSet_t iSet) {
	if (iSet < ACTIONSET_NONE || iSet > ACTIONSET_COUNT) {
		CFSTEAMINPUT_LOG_WARN("SetActionSet has been called with an out of range argument!\n");
		return;
	}

	const char* pszSetName = pszActionSetNames[iSet];
	if (!pszSetName || pszSetName[0] == '\0') {
		CFSTEAMINPUT_LOG_WARN("Failed to get an action set's internal name.\n");
		return;
	}

	InputActionSetHandle_t hSetHandle = input_->GetActionSetHandle(pszSetName);
	for (int i = 0; i < gamepadsCount_; i++) {
		InputHandle_t hGamepadHandle = gamepads_[i];
		CFSTEAMINPUT_LOG_INFO("Applying action set for controller #%d...", i);
		input_->ActivateActionSet(hGamepadHandle, hSetHandle);
	}
}

/*! \fn void CCFSteamInputManager::ShowBindingPanel(int iControllerIndex) 
 * \brief Shows the Steam Input controller configuration panel.
 * \param iControllerIndex The number of the connected controller.
 */
void CCFSteamInputManager::ShowBindingPanel(int iControllerIndex) {
	if (iControllerIndex < gamepadsCount_ || iControllerIndex > gamepadsCount_) {
		CFSTEAMINPUT_LOG_WARN("ShowBindingPanel has been called with an out of range argument!\n");
		return;
	}

	input_->ShowBindingPanel(gamepads_[iControllerIndex]);
}

static int CFSteamInputSettingsCompletion(char const* pszPartial, char pszCommands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH]) {
	int iCurrent = 0;

	const char* pszCmdName = "cf_steaminput_settings";
	char* pszSubString = NULL;
	int iSubStringLen = 0;
	if (Q_strstr(pszPartial, pszCmdName) && Q_strlen(pszPartial) > Q_strlen(pszCmdName) + 1)
	{
		pszSubString = (char*)pszPartial + Q_strlen(pszCmdName) + 1;
		iSubStringLen = Q_strlen(pszSubString);
	}

	int iConnectedCount = SteamInputManager()->GetConnectedGamepadsCount();

	int i = 0;
	char pszIndexStr[25];
	while (i <= iConnectedCount && iCurrent < COMMAND_COMPLETION_MAXITEMS)
	{
		if (!SteamInputManager()->IsControllerConnected(i)) {
			i++;
			continue;
		}

		itoa(i, pszIndexStr, 10);
		if (!pszSubString || !Q_strncasecmp(pszIndexStr, pszSubString, iSubStringLen))
		{
			Q_snprintf(pszCommands[iCurrent], sizeof(pszCommands[iCurrent]), "%s %s", pszCmdName, pszIndexStr);
			iCurrent++;
		}
		i++;
	}

	return iCurrent;
}

/**
 * \brief Shows the Steam Input controller configuration panel through the Steam Input manager.
 */
CON_COMMAND_F_COMPLETION(
	cf_steaminput_settings,
	"Shows the Steam Input controller configuration panel through the Steam Input manager.",
	FCVAR_CLIENTDLL,
	CFSteamInputSettingsCompletion) {
	if (args.ArgC() < 2) {
		Warning("Usage: cf_steaminput_settings <controller_index>\n");
		return;
	}

	const char* pszIndexStr = args[1];
	if (!pszIndexStr || pszIndexStr[0] == '\0') {
		Warning("Usage: cf_steaminput_settings <controller_index>\n");
		return;
	}

	int iIndex = Q_atoi(pszIndexStr);

	SteamInputManager()->ShowBindingPanel(iIndex);
}

/*! \fn bool CCFSteamInputManager::IsControllerConnected(int iControllerIndex) const
 * \brief Determines whether this controller by index is currently connected or not.
 * \param iControllerIndex The number of the connected controller.
 * \returns true if the specified index is currently active and connected, false otherwise.
 */
bool CCFSteamInputManager::IsControllerConnected(int iControllerIndex) const {
	return iControllerIndex > 0 && iControllerIndex <= gamepadsCount_; 
}

/*! \fn void CCFSteamInputManager::OnCommand(const char* pszCommand)
 * \brief Executes a command from \ref pszActionsNames.
 * \param pszCommand The value of the command string.
 */
void CCFSteamInputManager::OnCommand(const char* pszCommand) {

}

/*! \fn void CCFSteamInputManager::OnOSKHidden(GamepadTextInputDismissed_t* param)
 * \brief GamepadTextInputDimissed event.
 * \param param The event's parameters.
 */
void CCFSteamInputManager::OnOSKHidden(GamepadTextInputDismissed_t* param) {

}

static CCFSteamInputManager s_SteamInputMgr;

/*! \fn SteamInputManager()
 * \brief Singleton instance of the Steam Input manager.
 * \returns Returns a singleton instance of the Steam Input manager.
 */
CCFSteamInputManager* SteamInputManager() {
	return &s_SteamInputMgr;
}
