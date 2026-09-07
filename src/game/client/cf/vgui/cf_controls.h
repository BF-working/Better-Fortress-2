#ifndef CF_CONTROLS_H
#define CF_CONTROLS_H
#include <vgui/IScheme.h>
#include <vgui/KeyCode.h>
#include <KeyValues.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RichText.h>
#include "utlvector.h"
#include "vgui_controls/PHandle.h"
#include <vgui_controls/Tooltip.h>
#include "econ_controls.h"
#include "sc_hinticon.h"
#if defined( TF_CLIENT_DLL )
#include "tf_shareddefs.h"
#include "tf_imagepanel.h"
#endif
#include "vgui_controls/PropertyDialog.h"
#include <vgui_controls/Frame.h>
#include <../common/GameUI/scriptobject.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/CheckButton.h>
#include <vgui/IImage.h>

//Custom Fortress Res
#define RES_SERVERMENU	"resource/ui/custom/createserver/Menu.res"
#define RES_CREDITSMENU "resource/ui/custom/AuthorCredits.res"

namespace vgui
{
	//-----------------------------------------------------------------------------
	// Purpose: Simple IImage wrapper for workshop map preview textures
	//-----------------------------------------------------------------------------
	class CMapPreviewImage : public IImage
	{
	public:
		CMapPreviewImage();
		virtual ~CMapPreviewImage();
		
		void SetTextureRGBA(const byte* rgba, int width, int height);
		void Clear();
		bool IsValid() const { return m_bValid; }
		
		// IImage interface
		virtual void Paint() override;
		virtual void SetPos(int x, int y) override { m_nX = x; m_nY = y; }
		virtual void GetContentSize(int &wide, int &tall) override { wide = m_nWide; tall = m_nTall; }
		virtual void GetSize(int &wide, int &tall) override { wide = m_nWide; tall = m_nTall; }
		virtual void SetSize(int wide, int tall) override { m_nWide = wide; m_nTall = tall; }
		virtual void SetColor(Color col) override { m_Color = col; }
		virtual bool Evict() override { return false; }
		virtual int GetNumFrames() override { return 1; }
		virtual void SetFrame(int nFrame) override {}
		virtual HTexture GetID() override { return m_nTextureID; }
		virtual void SetRotation(int iRotation) override {}

	private:
		int m_nTextureID;
		int m_nX, m_nY;
		int m_nWide, m_nTall;
		int m_nImageWidth, m_nImageHeight;
		Color m_Color;
		bool m_bValid;
	};
} // namespace vgui

struct CreateServerMapItem {
	char mapName[MAX_MAP_NAME];
	bool bIsWorkshopMap;
	PublishedFileId_t iMapFileId;
};

//-----------------------------------------------------------------------------
// Purpose: Displays scrollable mod credits window
//-----------------------------------------------------------------------------
class CTFModCreditsDialog : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CTFModCreditsDialog, vgui::EditablePanel);

public:
	CTFModCreditsDialog(vgui::Panel* parent);
	~CTFModCreditsDialog();

	virtual void	ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void	ApplySettings(KeyValues* pResourceData);

	void	Deploy(void);

private:

	void CreateControls();
	void DestroyControls();

	virtual void OnCommand(const char* command);
	virtual void OnClose();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	virtual void OnKeyCodePressed(vgui::KeyCode code);

private:
	CInfoDescription* m_pDescription;
	mpcontrol_t* m_pList;
	vgui::PanelListPanel* m_pListPanel;
	CTFTextToolTip* m_pToolTip;
	vgui::EditablePanel* m_pToolTipEmbeddedPanel;

	CPanelAnimationVarAliasType(int, m_iControlW, "control_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iControlH, "control_h", "20", "proportional_int"); // We have to set this or it get's stompped by the resfile 
	CPanelAnimationVarAliasType(int, m_iSliderW, "slider_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iSliderH, "slider_h", "0", "proportional_int");
};

//-----------------------------------------------------------------------------
// Purpose: Displays the new create server menu
//-----------------------------------------------------------------------------
class CTFCreateServerDialog : public vgui::PropertyDialog
{
	DECLARE_CLASS_SIMPLE(CTFCreateServerDialog, vgui::PropertyDialog);

public:
	CTFCreateServerDialog(vgui::Panel* parent);
	~CTFCreateServerDialog();

	virtual void	ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void	ApplySettings(KeyValues* pResourceData);

	void	Deploy(void);

private:

	void CreateControls();
	void DestroyControls();
	void GatherCurrentValues();
	void SaveValues();
	void LoadMapList();
	void RefreshMapList();
	void RequestWorkshopPreview(PublishedFileId_t fileID);
	void FilterOptions();

	// HTTP callback for workshop preview image
	CCallResult<CTFCreateServerDialog, HTTPRequestCompleted_t> m_callbackHTTPPreview;
	void Steam_OnPreviewImageReceived(HTTPRequestCompleted_t* pResult, bool bError);

	virtual void OnCommand( const char *command );
	virtual void OnClose();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnThink();

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_INT( OnCheckButtonChecked, "CheckButtonChecked", state );

private:
	CInfoDescription	*m_pDescription;
	mpcontrol_t* m_pList;
	CTFTextToolTip* m_pToolTip;
	vgui::EditablePanel* m_pToolTipEmbeddedPanel;

	CUtlVector< vgui::PanelListPanel* > m_pPages;

	// Map filtering
	vgui::TextEntry* m_pMapSearchEntry;
	vgui::CheckButton* m_pWorkshopFilterCheck;
	//CUtlVector< CUtlString > m_vecAllMaps;
	//CUtlVector< bool > m_vecIsWorkshopMap;
	//CUtlVector< PublishedFileId_t > m_vecMapFileIDs;
	CUtlVector<CreateServerMapItem> m_vecAllMaps;
	char m_szLastSearchFilter[256];
	bool m_bLastWorkshopOnly;

	// General options filtering
	vgui::TextEntry* m_pOptionsSearchEntry;
	char m_szLastOptionsSearchFilter[256];

	// Workshop preview image
	HTTPRequestHandle m_hPendingPreviewRequest;
	PublishedFileId_t m_nCurrentPreviewFileID;
	vgui::CMapPreviewImage* m_pWorkshopPreviewImage;
	PublishedFileId_t m_nLastDisplayedMapFileID;

	CPanelAnimationVarAliasType(int, m_iControlW, "control_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iControlH, "control_h", "20", "proportional_int"); // Assume they want height 20
	CPanelAnimationVarAliasType(int, m_iSliderW, "slider_w", "0", "proportional_int");
	CPanelAnimationVarAliasType(int, m_iSliderH, "slider_h", "0", "proportional_int");
};

#endif // CF_CONTROLS_H