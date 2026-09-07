//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains most of the code for Custom Fortress Specific Controls.
//
//=============================================================================//

#include "cbase.h"

#include "ienginevgui.h"
#include <vgui_controls/ScrollBarSlider.h>
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "tf_controls.h"
#include "cf_controls.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/PropertySheet.h"
#include "econ_item_system.h"
#include "iachievementmgr.h"
#include "clientmode_tf.h"
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <../common/GameUI/cvarslider.h>
#include "filesystem.h"
#include "hud_controlpointicons.h"
#include "tf_statsummary.h"
#include "steam/steam_api.h"
#include "../cf/cf_workshop_manager.h"
#include "workshop/ugc_utils.h"
#include "utlbuffer.h"
#include "imageutils.h"

ConVar cl_map("cl_map", "-1");

using namespace vgui;

//-----------------------------------------------------------------------------
// CMapPreviewImage - Simple IImage wrapper for workshop map preview textures
//-----------------------------------------------------------------------------
CMapPreviewImage::CMapPreviewImage()
	: m_nTextureID(-1)
	, m_nX(0)
	, m_nY(0)
	, m_nWide(0)
	, m_nTall(0)
	, m_nImageWidth(0)
	, m_nImageHeight(0)
	, m_bValid(false)
{
	m_Color = Color(255, 255, 255, 255);
}

CMapPreviewImage::~CMapPreviewImage()
{
	Clear();
}

void CMapPreviewImage::SetTextureRGBA(const byte* rgba, int width, int height)
{
	if (!rgba || width <= 0 || height <= 0)
		return;
	
	if (m_nTextureID == -1)
	{
		m_nTextureID = surface()->CreateNewTextureID(true);
	}
	
	surface()->DrawSetTextureRGBAEx(m_nTextureID, rgba, width, height, IMAGE_FORMAT_RGBA8888);
	
	m_nImageWidth = width;
	m_nImageHeight = height;
	
	// Set size to image dimensions (ImagePanel will override via SetSize when scaling)
	m_nWide = width;
	m_nTall = height;
	
	m_bValid = true;
}

void CMapPreviewImage::Clear()
{
	if (m_nTextureID != -1)
	{
		surface()->DestroyTextureID(m_nTextureID);
		m_nTextureID = -1;
	}
	m_bValid = false;
	m_nImageWidth = 0;
	m_nImageHeight = 0;
}

void CMapPreviewImage::Paint()
{
	if (!m_bValid || m_nTextureID == -1)
		return;
	
	surface()->DrawSetTexture(m_nTextureID);
	surface()->DrawSetColor(m_Color);
	surface()->DrawTexturedRect(m_nX, m_nY, m_nX + m_nWide, m_nY + m_nTall);
}

static vgui::DHANDLE<CTFCreateServerDialog> g_pTFModServerDialog;
void CL_OpenTFModServerDialog(const CCommand& args)
{
	if (g_pTFModServerDialog.Get() == NULL)
	{
		g_pTFModServerDialog = vgui::SETUP_PANEL(new CTFCreateServerDialog(NULL));
	}
	g_pTFModServerDialog->Deploy();
}
static ConCommand modcreateserver("modcreateserver", &CL_OpenTFModServerDialog, "Displays the Custom Fortress Server Creation dialog.");


static vgui::DHANDLE<CTFModCreditsDialog> g_pTFModCreditsDialog;
void CL_OpenTFModCreditsDialog(const CCommand& args)
{
	if (g_pTFModCreditsDialog.Get() == NULL)
	{
		g_pTFModCreditsDialog = vgui::SETUP_PANEL(new CTFModCreditsDialog(NULL));
	}

	g_pTFModCreditsDialog->Deploy();
}
static ConCommand openmodcredits("openmodcredits", &CL_OpenTFModCreditsDialog, "Displays the Custom Fortress Server Creation dialog.");



#define CREATE_SERVER_DIR "cfg"
#define DEFAULT_CREATE_SERVER_FILE CREATE_SERVER_DIR "/server_options_default.txt"
#define CREATE_SERVER_FILE CREATE_SERVER_DIR "/server_options.txt"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFCreateServerDialog::CTFCreateServerDialog(vgui::Panel* parent) : PropertyDialog(NULL, "TFModServerDialog")
{
	// Need to use the clientscheme (we're not parented to a clientscheme'd panel)

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(enginevgui->GetPanel(PANEL_CLIENTDLL), "resource/ClientScheme.res", "ClientScheme");
	SetScheme(scheme);
	SetProportional(true);

	m_pList = NULL;
	m_pMapSearchEntry = NULL;
	m_pWorkshopFilterCheck = NULL;
	m_pOptionsSearchEntry = NULL;
	m_szLastSearchFilter[0] = '\0';
	m_szLastOptionsSearchFilter[0] = '\0';
	m_bLastWorkshopOnly = false;
	m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
	m_nCurrentPreviewFileID = 0;
	m_pWorkshopPreviewImage = new CMapPreviewImage();
	m_nLastDisplayedMapFileID = 0;

	m_pToolTip = new CTFTextToolTip(this);
	m_pToolTipEmbeddedPanel = new vgui::EditablePanel(this, "TooltipPanel");
	m_pToolTipEmbeddedPanel->SetKeyBoardInputEnabled(false);
	m_pToolTipEmbeddedPanel->SetMouseInputEnabled(false);
	m_pToolTip->SetEmbeddedPanel(m_pToolTipEmbeddedPanel);
	m_pToolTip->SetTooltipDelay(0);

	m_pDescription = new CInfoDescription();

	// If this can be simplified, I'd gladly take it.

	KeyValuesAD pFileKV( "OPTIONS" );
	if ( !pFileKV->LoadFromFile( g_pFullFileSystem, CREATE_SERVER_FILE, "MOD" ) && !pFileKV->LoadFromFile( g_pFullFileSystem, DEFAULT_CREATE_SERVER_FILE, "MOD" ) )
		return;

	int i = 0;
	for ( KeyValues *pCurTab = pFileKV->GetFirstSubKey(); pCurTab; pCurTab = pCurTab->GetNextKey() )
	{
		// 1st layer: Tabs
		const char *pTabName = pCurTab->GetName();
		m_pPages.AddToTail( new vgui::PanelListPanel(this, pTabName) );
		AddPage(m_pPages[i], pTabName);
		Warning("Adding page: %s\n", pTabName);
		for (KeyValues* pCurOption = pCurTab->GetFirstSubKey(); pCurOption; pCurOption = pCurOption->GetNextKey())
		{
			// 2nd layer: Options in tab
			const char *pOptionName = pCurOption->GetName();
			CScriptObject *pObj = new CScriptObject();

			char type[64];
			const char *pParamType = pCurOption->GetString( "type" );
			Q_strncpy( type, pParamType, sizeof( type ) );
			Q_strncpy( pObj->cvarname, pOptionName, sizeof( pObj->cvarname ) );
			Q_strncpy( pObj->prompt, pCurOption->GetString( "label", "Unnamed" ), sizeof( pObj->prompt ) );
			Q_strncpy( pObj->tooltip, pCurOption->GetString( "tooltip" ), sizeof( pObj->tooltip ) );
			Q_strncpy( pObj->defValue, pCurOption->GetString( "val" ), sizeof( pObj->defValue ) );
			Q_strncpy( pObj->curValue, pObj->defValue, sizeof( pObj->curValue ) );
			pObj->fdefValue = atof( pObj->defValue );

			pObj->type = pObj->GetType( type );

			for ( KeyValues *pCurParam = pCurOption->GetFirstSubKey(); pCurParam; pCurParam = pCurParam->GetNextKey() )
			{
				const char *pParamName = pCurParam->GetName();
				if (!V_stricmp(pParamName, "options"))
				{
					for ( KeyValues *pCurListItem = pCurParam->GetFirstSubKey(); pCurListItem; pCurListItem = pCurListItem->GetNextKey() )
					{
						CScriptListItem *pItem;
						pItem = new CScriptListItem( pCurListItem->GetName(), pCurListItem->GetString() );
						pObj->AddItem( pItem );
					}
				}

			}
			pObj->objParent = m_pPages[i];
			m_pDescription->AddObject( pObj );
		}
		i++;
	}

	// do not init this way as it's harder to get tabs working
	//m_pDescription->InitFromFile( DEFAULT_CREATE_SERVER_FILE );
	//m_pDescription->InitFromFile( CREATE_SERVER_FILE, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFCreateServerDialog::~CTFCreateServerDialog()
{
	delete m_pDescription;
	delete m_pWorkshopPreviewImage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	CreateControls();
	LoadControlSettings( RES_SERVERMENU );

	// Find the options search entry control
	m_pOptionsSearchEntry = dynamic_cast<TextEntry*>( FindChildByName( "OptionsSearchEntry" ) );
	if ( m_pOptionsSearchEntry )
	{
		m_pOptionsSearchEntry->AddActionSignalTarget( this );
	}

	FOR_EACH_VEC(m_pPages, i)
	{
		m_pPages[i]->SetFirstColumnWidth(0);
		m_pPages[i]->SetVisible( false );
	}
	m_pPages[0]->SetVisible( true );

	SetOKButtonVisible(false);
	SetCancelButtonVisible(false);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::ApplySettings(KeyValues* inResourceData)
{
	BaseClass::ApplySettings(inResourceData);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::OnClose()
{
	BaseClass::OnClose();
	
	TFModalStack()->PopModal(this);
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::OnCommand(const char* command)
{
	if (!stricmp(command, "Ok"))
	{
		OnClose();
		return;
	}
	else if (!stricmp(command, "Close"))
	{
		SaveValues();
		OnClose();
		return;
	}
	else if (!stricmp(command, "FixUI"))
	{
		// Reset server_options.txt by copying from default
		if (g_pFullFileSystem->FileExists(DEFAULT_CREATE_SERVER_FILE, "MOD"))
		{
			// Delete existing server_options.txt
			g_pFullFileSystem->RemoveFile(CREATE_SERVER_FILE, "MOD");
			
			// Reload everything
			DestroyControls();
			
			// Reinitialize from default file
			if (m_pDescription)
			{
				delete m_pDescription;
				m_pDescription = new CInfoDescription();
			}
			
			m_pList = NULL;
			m_pPages.RemoveAll();
			
			KeyValuesAD pFileKV("OPTIONS");
			if (pFileKV->LoadFromFile(g_pFullFileSystem, DEFAULT_CREATE_SERVER_FILE, "MOD"))
			{
				int i = 0;
				for (KeyValues *pCurTab = pFileKV->GetFirstSubKey(); pCurTab; pCurTab = pCurTab->GetNextKey())
				{
					const char *pTabName = pCurTab->GetName();
					m_pPages.AddToTail(new vgui::PanelListPanel(this, pTabName));
					AddPage(m_pPages[i], pTabName);
					
					for (KeyValues* pCurOption = pCurTab->GetFirstSubKey(); pCurOption; pCurOption = pCurOption->GetNextKey())
					{
						const char *pOptionName = pCurOption->GetName();
						CScriptObject *pObj = new CScriptObject();
						
						char type[64];
						const char *pParamType = pCurOption->GetString("type");
						Q_strncpy(type, pParamType, sizeof(type));
						Q_strncpy(pObj->cvarname, pOptionName, sizeof(pObj->cvarname));
						Q_strncpy(pObj->prompt, pCurOption->GetString("label", "Unnamed"), sizeof(pObj->prompt));
						Q_strncpy(pObj->tooltip, pCurOption->GetString("tooltip"), sizeof(pObj->tooltip));
						Q_strncpy(pObj->defValue, pCurOption->GetString("val"), sizeof(pObj->defValue));
						Q_strncpy(pObj->curValue, pObj->defValue, sizeof(pObj->curValue));
						pObj->fdefValue = atof(pObj->defValue);
						
						pObj->type = pObj->GetType(type);
						
						for (KeyValues *pCurParam = pCurOption->GetFirstSubKey(); pCurParam; pCurParam = pCurParam->GetNextKey())
						{
							const char *pParamName = pCurParam->GetName();
							if (!V_stricmp(pParamName, "options"))
							{
								for (KeyValues *pCurListItem = pCurParam->GetFirstSubKey(); pCurListItem; pCurListItem = pCurListItem->GetNextKey())
								{
									CScriptListItem *pItem;
									pItem = new CScriptListItem(pCurListItem->GetName(), pCurListItem->GetString());
									pObj->AddItem(pItem);
								}
							}
						}
						pObj->objParent = m_pPages[i];
						m_pDescription->AddObject(pObj);
					}
					i++;
				}
			}
			
			// Recreate controls with default values
			CreateControls();
			LoadMapList();
			InvalidateLayout();
		}
		OnClose();
		return;
	}
	else if (!stricmp(command, "CreateServer"))
	{
		SaveValues();
		if ( m_pDescription )
		{
			m_pDescription->WriteToConfig();
			CScriptObject* pMapInfoObj = m_pDescription->FindObject("cl_map");
			if ( pMapInfoObj )
			{
				CScriptListItem *pItem = pMapInfoObj->pListItems;

				if ( pItem )
				{
					while ( pItem )
					{
						// This is horrible.
						if (!Q_stricmp(pItem->szValue, pMapInfoObj->curValue))
						{
							if(!Q_stricmp(pItem->szItemText, "#GameUI_RandomMap"))
							{
								CScriptListItem *p;
								int c = 0;
								p = pMapInfoObj->pListItems;
								while ( p )
								{
									p = p->pNext;
									c++;
								}

								int v = RandomInt(1, c); // Ignore the RandomMap option
								p = pMapInfoObj->pListItems;
								c = 0;
								while ( p )
								{
									if(c == v)
										break;
									p = p->pNext;
									c++;
								}
								pItem = p;
							}
							break;
						}

						pItem = pItem->pNext;
					}
					// Show the stats summary panel as a loading screen before changing level
					CTFStatsSummaryPanel *pStatsPanel = GStatsSummaryPanel();
					if ( pStatsPanel )
					{
						pStatsPanel->OnMapLoad( pItem->szItemText );
						pStatsPanel->SetVisible( true );
						pStatsPanel->MoveToFront();
					}
					
					// Check if this is a Workshop map by looking for the map in our list
					bool bIsWorkshopMap = false;
					PublishedFileId_t workshopFileID = 0;
					FOR_EACH_VEC( m_vecAllMaps, i )
					{
						CreateServerMapItem item = m_vecAllMaps[i];
						const char* pszMapName = item.mapName;

						if ( Q_stricmp( pszMapName, pItem->szItemText ) == 0 )
						{
							bIsWorkshopMap = item.bIsWorkshopMap;
							workshopFileID = item.iMapFileId;
							break;
						}
					}
					
					// Use host_workshop_map for Workshop maps, regular map command for others
					if ( bIsWorkshopMap && workshopFileID != 0 )
					{
						engine->ClientCmd_Unrestricted(CFmtStr("progress_enable; host_workshop_map %llu", workshopFileID));
					}
					else
					{
						engine->ClientCmd_Unrestricted(CFmtStr("progress_enable; map %s", pItem->szItemText));
					}
				}
			}
		}
		OnClose();
		return;
	}

	BaseClass::OnCommand(command);
}

const char* GetTypeName( int type ) 
{
	switch (type)
	{
		case O_BOOL:
			return "BOOL";
		case O_NUMBER:
			return "NUMBER";
		case O_LIST:
			return "LIST";
		case O_STRING:
			return "STRING";
		case O_SLIDER:
			return "SLIDER";
		case O_CATEGORY:
			return "CATEGORY";
		case O_BUTTON:
			return "BUTTON";
		default:
		case O_OBSOLETE:
			return "OBSOLETE";
	}
}

void CTFCreateServerDialog::SaveValues() 
{
	// Get the values from the controls:
	GatherCurrentValues();

	// Create the game.cfg file
	if ( m_pDescription )
	{
		FileHandle_t fp;

		KeyValuesAD pFileKV( "OPTIONS" );

		mpcontrol_t *pList;

		CScriptObject *pObj;
		CScriptListItem *pItem;

		char szValue[256];
		char strValue[ 256 ];

		pList = m_pList;

		// This is a mess of sphagetti code, someone please simplify this!

		FOR_EACH_VEC(m_pPages, i)
		{
			KeyValues *pTabKV = new KeyValues( m_pPages[i]->GetName() );
			pTabKV->SetString( "NO OPTIONS", "" );	// Ading this to force the tab to show up
			pFileKV->AddSubKey( pTabKV );
		}

		while ( pList )
		{
			pObj = pList->pScrObj;
			DevMsg( "CVAR: %s LABEL: %s TOOLTIP: %s TYPE: %s VAL: %s PARENT: %s\n", pObj->cvarname, pObj->prompt, pObj->tooltip, GetTypeName(pObj->type), pObj->curValue, pObj->objParent->GetName() );

			// If there's already an entry with this index, overwrite the data.
			KeyValues *pTabKV = pFileKV->FindKey( pObj->objParent->GetName() );
			if ( !pTabKV )
			{
				pTabKV = new KeyValues( pObj->objParent->GetName() );
				pFileKV->AddSubKey( pTabKV );
			}

			KeyValues *pCvarKV = new KeyValues( pObj->cvarname );
			pCvarKV->SetString( "label", pObj->prompt );
			pCvarKV->SetString( "tooltip", pObj->tooltip );
			pCvarKV->SetString( "type", GetTypeName( pObj->type ) );
			if (pObj->type == O_LIST)
			{
				KeyValues *pOptionsKV = new KeyValues( "options" );
				CScriptListItem *pItem = pObj->pListItems;
				if ( pItem )
				{
					while ( pItem )
					{
						pOptionsKV->SetString( pItem->szItemText, pItem->szValue );
						pItem = pItem->pNext;
					}
					pCvarKV->AddSubKey( pOptionsKV );
				}
			}
			pCvarKV->SetString( "val", pObj->curValue );
			pTabKV->AddSubKey( pCvarKV );


			KeyValues *pTempKV = pTabKV->FindKey( "NO OPTIONS" );
			if( pTempKV )
			{
				pTabKV->RemoveSubKey( pTempKV );
			}
			pList = pList->next;
		}

		g_pFullFileSystem->CreateDirHierarchy( CREATE_SERVER_DIR );
		fp = g_pFullFileSystem->Open( CREATE_SERVER_FILE, "wb" );
		if ( fp )
		{
			pFileKV->SaveToFile(g_pFullFileSystem, CREATE_SERVER_FILE);
			g_pFullFileSystem->Close( fp );
		}

		// Add settings to config.cfg
		//m_pDescription->WriteToConfig();

		/*g_pFullFileSystem->CreateDirHierarchy(CREATE_SERVER_DIR);
		fp = g_pFullFileSystem->Open( CREATE_SERVER_FILE, "wb" );
		if ( fp )
		{
			m_pDescription->WriteToScriptFile( fp );
			g_pFullFileSystem->Close( fp );
		}*/
	}
}

void CTFCreateServerDialog::OnKeyCodeTyped(KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if (code == KEY_ESCAPE)
	{
		OnClose();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::OnKeyCodePressed(KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if (GetBaseButtonCode(code) == KEY_XBUTTON_B || GetBaseButtonCode(code) == STEAMCONTROLLER_B || GetBaseButtonCode(code) == STEAMCONTROLLER_START)
	{
		OnClose();
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::GatherCurrentValues()
{
	if ( !m_pDescription )
		return;

	// OK
	CheckButton *pBox;
	TextEntry *pEdit;
	ComboBox *pCombo;
	CCvarSlider *pSlider;

	mpcontrol_t *pList;

	CScriptObject *pObj;
	CScriptListItem *pItem;

	char szValue[256];
	char strValue[ 256 ];

	pList = m_pList;
	while ( pList )
	{
		pObj = pList->pScrObj;

		if ( pObj->type == O_CATEGORY || pObj->type == O_BUTTON )
		{
			pList = pList->next;
			continue;
		}

		if ( !pList->pControl )
		{
			pObj->SetCurValue( pObj->defValue );
			pList = pList->next;
			continue;
		}

		switch ( pObj->type )
		{
		case O_BOOL:
			pBox = (CheckButton *)pList->pControl;
			sprintf( szValue, "%s", pBox->IsSelected() ? "1" : "0" );
			break;
		case O_NUMBER:
			pEdit = ( TextEntry * )pList->pControl;
			pEdit->GetText( strValue, sizeof( strValue ) );
			sprintf( szValue, "%s", strValue );
			break;
		case O_STRING:
			pEdit = ( TextEntry * )pList->pControl;
			pEdit->GetText( strValue, sizeof( strValue ) );
			sprintf( szValue, "%s", strValue );
			break;
		case O_LIST:
			{
			pCombo = (ComboBox *)pList->pControl;
			// pCombo->GetText( strValue, sizeof( strValue ) );
			int activeItem = pCombo->GetActiveItem();

			pItem = pObj->pListItems;
			//			int n = (int)pObj->fdefValue;

			while ( pItem )
			{
				if (!activeItem--)
					break;

				pItem = pItem->pNext;
			}

			if ( pItem )
			{
				sprintf( szValue, "%s", pItem->szValue );
			}
			else  // Couln't find index
			{
				//assert(!("Couldn't find string in list, using default value"));
				sprintf( szValue, "%s", pObj->defValue );
			}
			break;
		}
		case O_SLIDER:
			pSlider = ( CCvarSlider * )pList->pControl;
			sprintf( szValue, "%.2f", pSlider->GetSliderValue() );
			break;
		}

		// Remove double quotes and % characters
		UTIL_StripInvalidCharacters( szValue, sizeof(szValue) );

		V_strcpy_safe( strValue, szValue );

		pObj->SetCurValue( strValue );

		pList = pList->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::CreateControls()
{
	DestroyControls();

	// Go through desciption creating controls
	CScriptObject *pObj;

	pObj = m_pDescription->pObjList;

	// Load maps and build the dropdown
	LoadMapList();
	RefreshMapList();

	mpcontrol_t	*pCtrl;

	Button *pButton;
	CheckButton *pBox;
	TextEntry *pEdit;
	ComboBox *pCombo;
	CCvarSlider *pSlider;
	CScriptListItem *pListItem;

	//Panel *objParent = m_pPageOne;

	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	vgui::HFont hTextFont = pScheme->GetFont( "HudFontSmallestBold", true );
	Color tanDark = pScheme->GetColor( "TanDark", Color(255,0,0,255) );

	Warning( "CreateControls: Starting to process objects, pObj=%p\n", pObj );

	while ( pObj )
	{
		Warning( "CreateControls: Processing '%s' type=%d\n", pObj->cvarname, pObj->type );
		
		if ( pObj->type == O_OBSOLETE )
		{
			pObj = pObj->pNext;
			continue;
		}

		Panel *objParent = pObj->objParent;
		PanelListPanel *objParentList = (PanelListPanel *) pObj->objParent;

		pCtrl = new mpcontrol_t( objParent, pObj->cvarname);
		pCtrl->type = pObj->type;

		// Force it to invalidate scheme now, so we can change color afterwards and have it persist
		pCtrl->InvalidateLayout( true, true );

		switch ( pCtrl->type )
		{
		case O_BOOL:
			pBox = new CheckButton( pCtrl, pObj->cvarname, pObj->prompt );
			pBox->SetSelected( pObj->fdefValue != 0.0f ? true : false );

			pCtrl->pControl = (Panel *)pBox;
			pBox->SetFont( hTextFont );

			pBox->InvalidateLayout( true, true );

			pBox->SetFgColor( tanDark );
			pBox->SetDefaultColor( tanDark, pBox->GetBgColor() );
			pBox->SetArmedColor( tanDark, pBox->GetBgColor() );
			pBox->SetDepressedColor( tanDark, pBox->GetBgColor() );
			pBox->SetSelectedColor( tanDark, pBox->GetBgColor() );
			pBox->SetHighlightColor( tanDark );
			pBox->GetCheckImage()->SetColor( tanDark );
			break;
		case O_STRING:
		case O_NUMBER:
			pEdit = new TextEntry( pCtrl, pObj->cvarname);
			pEdit->InsertString(pObj->defValue);
			pCtrl->pControl = (Panel *)pEdit;
			pEdit->SetFont( hTextFont );

			pEdit->InvalidateLayout( true, true );
			pEdit->SetBgColor( Color(0,0,0,255) );
			break;
		case O_LIST:
			{
			pCombo = new ComboBox( pCtrl, pObj->cvarname, 5, false );

			// track which row matches the current value
			int iRow = -1;
			int iCount = 0;
			pListItem = pObj->pListItems;
			while ( pListItem )
			{
				if ( iRow == -1 && !Q_stricmp( pListItem->szValue, pObj->curValue ) )
					iRow = iCount;

				pCombo->AddItem( pListItem->szItemText, NULL );
				pListItem = pListItem->pNext;
				++iCount;
			}


			pCombo->ActivateItemByRow( iRow );

			pCtrl->pControl = (Panel *)pCombo;
			pCombo->SetFont( hTextFont );
		}
			break;
		case O_SLIDER:
			pSlider = new CCvarSlider( pCtrl, pObj->cvarname, "Test", pObj->fMin, pObj->fMax, pObj->cvarname, false );
			pCtrl->pControl = (Panel *)pSlider;
			break;
		case O_BUTTON:
			pButton = new CExButton( pCtrl, pObj->cvarname, pObj->prompt, this, pObj->defValue );
			pButton->SetFont( hTextFont );
			pCtrl->pControl = (Panel *)pButton;
			break;
		case O_CATEGORY:
			pCtrl->SetBorder( pScheme->GetBorder("OptionsCategoryBorder") );
			break;
		default:
			break;
		}

		if ( pCtrl->type != O_BOOL && pCtrl->type != O_BUTTON )
		{
			pCtrl->pPrompt = new vgui::Label( pCtrl, CFmtStr( "%s_DescLabel", pObj->cvarname ), "" );
			pCtrl->pPrompt->SetContentAlignment( vgui::Label::a_west );
			pCtrl->pPrompt->SetTextInset( 5, 0 );
			pCtrl->pPrompt->SetText( pObj->prompt );
			pCtrl->pPrompt->SetFont( hTextFont );

			pCtrl->pPrompt->InvalidateLayout( true, true );

			if ( pCtrl->type == O_CATEGORY )
			{
				pCtrl->pPrompt->SetFont( pScheme->GetFont( "HudFontSmallBold", true ) );
				pCtrl->pPrompt->SetFgColor( pScheme->GetColor( "TanLight", Color(255,0,0,255) ) );
			}
			else
			{
				pCtrl->pPrompt->SetFgColor( tanDark );
			}
		}

		pCtrl->pScrObj = pObj;

		switch ( pCtrl->type )
		{
		case O_BOOL:
		case O_STRING:
		case O_NUMBER:
		case O_LIST:
		case O_BUTTON:
		case O_CATEGORY:
			pCtrl->SetSize( m_iControlW, m_iControlH );
			break;
		case O_SLIDER:
			pCtrl->SetSize( m_iSliderW, m_iSliderH );
			break;
		default:
			break;
		}

		// Hook up the tooltip, if the entry has one
		if ( pObj->tooltip && pObj->tooltip[0] )
		{
			if ( pCtrl->pPrompt )
			{
				pCtrl->pPrompt->SetTooltip( m_pToolTip, pObj->tooltip );
			}
			else
			{
				pCtrl->SetTooltip( m_pToolTip, pObj->tooltip );
				pCtrl->pControl->SetTooltip( m_pToolTip, pObj->tooltip );
			}
		}

		objParentList->AddItem( NULL, pCtrl );

		// Store references to the map filter controls
		if ( !Q_stricmp( pObj->cvarname, "cl_map_search" ) && pCtrl->pControl )
		{
			m_pMapSearchEntry = dynamic_cast<TextEntry*>( pCtrl->pControl );
			if ( m_pMapSearchEntry )
			{
				Warning( "Found map search entry: %p\n", m_pMapSearchEntry );
				m_pMapSearchEntry->AddActionSignalTarget( this );
				m_pMapSearchEntry->SendNewLine( true );  // Send signal on enter key
			}
		}
		else if ( !Q_stricmp( pObj->cvarname, "cl_map_workshop_only" ) && pCtrl->pControl )
		{
			m_pWorkshopFilterCheck = dynamic_cast<CheckButton*>( pCtrl->pControl );
			if ( m_pWorkshopFilterCheck )
			{
				Warning( "Found workshop filter check: %p\n", m_pWorkshopFilterCheck );
				m_pWorkshopFilterCheck->AddActionSignalTarget( this );
			}
		}

		// Link it in
		if ( !m_pList )
		{
			m_pList = pCtrl;
			pCtrl->next = NULL;
		}
		else
		{
			mpcontrol_t *p;
			p = m_pList;
			while ( p )
			{
				if ( !p->next )
				{
					p->next = pCtrl;
					pCtrl->next = NULL;
					break;
				}
				p = p->next;
			}
		}

		pObj = pObj->pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::DestroyControls()
{
	mpcontrol_t* p, * n;

	// Clear references to filter controls
	m_pMapSearchEntry = NULL;
	m_pWorkshopFilterCheck = NULL;

	p = m_pList;
	while (p)
	{
		n = p->next;
		//
		if (p->pControl)
		{
			p->pControl->MarkForDeletion();
			p->pControl = NULL;
		}
		if (p->pPrompt)
		{
			p->pPrompt->MarkForDeletion();
			p->pPrompt = NULL;
		}
		delete p;
		p = n;
	}

	m_pList = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Loads all maps from disk into the map list
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::LoadMapList()
{
	m_vecAllMaps.RemoveAll();

	// Helper lambda to check if a map name already exists
	// Compares base name (strips workshop/ prefix and .ugcXXXX suffix)
	auto MapExists = [this]( const char* szMapName ) -> bool
	{
		// Extract base name from input
		char szBaseName[MAX_PATH];
		Q_strncpy( szBaseName, szMapName, sizeof( szBaseName ) );
		
		// Strip workshop/ prefix if present
		const char* pBase = szBaseName;
		char pszPrefix[96];
		Q_strcpy(pszPrefix, "workshop\\");
		Q_FixSlashes(pszPrefix);

		if ( Q_strnicmp( pBase, pszPrefix, 9 ) == 0 )
			pBase = szBaseName + 9;
		
		// Strip .ugcXXXX suffix if present
		char* pUgc = Q_stristr( (char*)pBase, ".ugc" );
		if ( pUgc )
			*pUgc = '\0';
		
		FOR_EACH_VEC( m_vecAllMaps, i )
		{
			CreateServerMapItem item = m_vecAllMaps[i];
			const char* pszMapName = item.mapName;

			// Extract base name from existing entry
			char szExistingBase[MAX_PATH];
			Q_strncpy( szExistingBase, pszMapName, sizeof( szExistingBase ) );
			
			const char* pExisting = szExistingBase;
			if ( Q_strnicmp( pExisting, pszPrefix, 9 ) == 0 )
				pExisting = szExistingBase + 9;
			
			char* pExistingUgc = Q_stristr( (char*)pExisting, ".ugc" );
			if ( pExistingUgc )
				*pExistingUgc = '\0';
			
			if ( Q_stricmp( pExisting, pBase ) == 0 )
				return true;
		}
		return false;
	};

	// =====================================================
	// FIRST: Scan workshop sources (they have proper file IDs and metadata)
	// =====================================================

	// Get workshop maps from the workshop manager
	CUtlVector<CCFWorkshopItem*> workshopMaps;
	CFWorkshop()->GetItemsByType( CF_WORKSHOP_TYPE_MAP, workshopMaps );
	Warning( "LoadMapList: Workshop manager has %d map items\n", workshopMaps.Count() );

	FOR_EACH_VEC(workshopMaps, i) {
		CCFWorkshopItem* pItem = workshopMaps[i];
		if (!pItem) continue;

		char pszInstallPath[MAX_PATH];
		if (!pItem->GetInstallPath(pszInstallPath, sizeof(pszInstallPath))) continue;

		char pszSearchDirPath[MAX_PATH];

		Q_snprintf(pszSearchDirPath, sizeof(pszSearchDirPath), "%s\\*.bsp", pszInstallPath);
		Q_FixSlashes(pszSearchDirPath);

		FileFindHandle_t hFindHandle;

		const char* pszFilePath = filesystem->FindFirstEx(pszSearchDirPath, "MOD", &hFindHandle);
		while (pszFilePath) {
			// 0% chance of it retrieving a directory, but better safe than sorry.
			if (!filesystem->FindIsDirectory(hFindHandle)) pszFilePath = filesystem->FindNext(hFindHandle); continue;

			// @PracticeMedicine:
			// Pretty sure that calling Q_StripExtension while setting the same char[] variable
			// worked out for them but honestly i'm mostly sure that it is just unsafe doing that,
			// so use Q_FileBase for "pszFileName" instead.
			//
			// For reference:
			// this is how they did it previously: Q_StripExtension(pszShortName, pszShortName, sizeof(pszShortName));
			char pszFileName[MAX_PATH];
			Q_FileBase(pszFilePath, pszFileName, sizeof(pszFileName));

			char pszShortName[MAX_PATH];
			Q_snprintf(pszShortName, sizeof(pszShortName), "workshop\\%s", pszFileName);
			Q_FixSlashes(pszShortName);

			char pszNameWithId[MAX_PATH];
			Q_snprintf(pszNameWithId, sizeof(pszNameWithId), "%s.ugc%llu", pszShortName, pItem->GetFileID());

			if (MapExists(pszNameWithId)) pszFilePath = filesystem->FindNext(hFindHandle); continue;

			CreateServerMapItem newItem{};
			Q_strncpy(newItem.mapName, pszNameWithId, sizeof(newItem.mapName));
			newItem.bIsWorkshopMap = true;
			newItem.iMapFileId = pItem->GetFileID();
			m_vecAllMaps.AddToTail(newItem);

			pszFilePath = filesystem->FindNext(hFindHandle);
		}

		filesystem->FindClose(hFindHandle);
	}

	ISteamUGC* pUGC = GetSteamUGC();
	if (pUGC) {
		uint32 iNumSubbed = pUGC->GetNumSubscribedItems();
		if (iNumSubbed > 0) {
			PublishedFileId_t* items = new PublishedFileId_t[iNumSubbed];
			if (pUGC->GetSubscribedItems(items, sizeof(items)) > 0) {
				for (int i = 0; i < sizeof(items); i++) {
					PublishedFileId_t itemId = items[i];
					char pszInstallPath[MAX_PATH];
					uint64 iSize = 0;
					uint32 iTime = 0;

					if (!pUGC->GetItemInstallInfo(itemId, &iSize, pszInstallPath, sizeof(pszInstallPath), &iTime))
						continue;

					char pszSearchPath[MAX_PATH];
					Q_snprintf(pszSearchPath, sizeof(pszSearchPath), "%s\\*.bsp", pszInstallPath);
					Q_FixSlashes(pszSearchPath);

					FileFindHandle_t hFindHandle;
					const char* pszFilePath = filesystem->FindFirstEx(pszSearchPath, "MOD", &hFindHandle);
					while (pszFilePath) {
						if (!filesystem->FindIsDirectory(hFindHandle)) continue;

						char pszFileName[MAX_PATH];
						Q_FileBase(pszFilePath, pszFileName, sizeof(pszFileName));

						char pszShortName[MAX_PATH];
						Q_snprintf(pszShortName, sizeof(pszShortName), "workshop\\%s", pszFileName);
						Q_FixSlashes(pszShortName);

						char pszStrippedShortName[MAX_PATH];
						Q_StripExtension(pszShortName, pszStrippedShortName, sizeof(pszStrippedShortName));

						char pszNameWithId[MAX_PATH];
						Q_snprintf(pszNameWithId, sizeof(pszNameWithId), "%s.ugc%llu", pszShortName, itemId);

						if (MapExists(pszNameWithId)) pszFilePath = filesystem->FindNext(hFindHandle); continue;

						CreateServerMapItem newItem{};
						Q_strncpy(newItem.mapName, pszNameWithId, sizeof(newItem.mapName));
						newItem.bIsWorkshopMap = true;
						newItem.iMapFileId = itemId;
						m_vecAllMaps.AddToTail(newItem);

						pszFilePath = filesystem->FindNext(hFindHandle);
					}
				}
			}
			delete[] items;
		}
	}
	
	// =====================================================
	// SECOND: Scan regular map folders (skip workshop maps already found)
	// =====================================================

	FileFindHandle_t mapHandle;

	char pszMapSearchDir[MAX_PATH];
	Q_strncpy(pszMapSearchDir, "maps/*.bsp", sizeof(pszMapSearchDir));
	Q_FixSlashes(pszMapSearchDir);

	const char* pMapFileName = filesystem->FindFirstEx( pszMapSearchDir, "GAME", &mapHandle );

	while (pMapFileName && pMapFileName[0] != '\0')
	{
		if (filesystem->FindIsDirectory(mapHandle))
		{
			pMapFileName = filesystem->FindNext(mapHandle);
			continue;
		}

		char szShortName[MAX_PATH];
		Q_strncpy(szShortName, pMapFileName, sizeof(szShortName));

		char szStrippedShortName[MAX_PATH];
		Q_StripExtension(szShortName, szStrippedShortName, sizeof(szStrippedShortName));

		if (MapExists(szStrippedShortName)) continue;

		CreateServerMapItem newItem{};
		Q_strncpy(newItem.mapName, szStrippedShortName, sizeof(newItem.mapName));
		newItem.bIsWorkshopMap = false;
		newItem.iMapFileId = 0;

		m_vecAllMaps.AddToTail(newItem);

		pMapFileName = filesystem->FindNext(mapHandle);
	}

	filesystem->FindClose( mapHandle );

	Q_strncpy(pszMapSearchDir, "maps/workshop/*.bsp", sizeof(pszMapSearchDir));
	Q_FixSlashes(pszMapSearchDir);

	// Also search workshop folder via filesystem (for any we might have missed)
	pMapFileName = filesystem->FindFirstEx(pszMapSearchDir, "MOD", &mapHandle);

	while (pMapFileName && pMapFileName[0] != '\0')
	{
		if (filesystem->FindIsDirectory(mapHandle))
		{
			pMapFileName = filesystem->FindNext(mapHandle);
			continue;
		}

		char szShortName[MAX_PATH];
		Q_snprintf(szShortName, sizeof(szShortName), "workshop/%s", pMapFileName);

		char szStrippedShortName[MAX_PATH];
		Q_StripExtension(szShortName, szStrippedShortName, sizeof(szStrippedShortName));

		if (MapExists(szStrippedShortName)) continue;

		PublishedFileId_t fileID = CFWorkshop()->GetMapIDFromName(szShortName);

		CreateServerMapItem newItem{};
		Q_strncpy(newItem.mapName, szStrippedShortName, sizeof(newItem.mapName));
		newItem.bIsWorkshopMap = true;
		newItem.iMapFileId = fileID;

		m_vecAllMaps.AddToTail(newItem);

		pMapFileName = filesystem->FindNext(mapHandle);
	}

	filesystem->FindClose( mapHandle );

	// Count workshop maps
	int nWorkshopCount = 0;
	FOR_EACH_VEC(m_vecAllMaps, i)
	{
		CreateServerMapItem item = m_vecAllMaps[i];
		if (!item.bIsWorkshopMap) continue;
		nWorkshopCount++;
	}
	Warning( "LoadMapList: Loaded %d maps, %d are workshop maps\n", m_vecAllMaps.Count(), nWorkshopCount );
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the map dropdown based on search/filter
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::RefreshMapList()
{
	CScriptObject *pMapInfoObj = m_pDescription->FindObject( "cl_map" );
	if ( !pMapInfoObj )
		return;

	// Get the search filter text
	char szSearchFilter[256] = { 0 };
	if ( m_pMapSearchEntry )
	{
		m_pMapSearchEntry->GetText( szSearchFilter, sizeof( szSearchFilter ) );
		Q_strlower( szSearchFilter );
	}

	// Get the workshop filter state
	bool bWorkshopOnly = false;
	if ( m_pWorkshopFilterCheck )
	{
		bWorkshopOnly = m_pWorkshopFilterCheck->IsSelected();
	}

	Warning( "RefreshMapList: search='%s', workshopOnly=%d, totalMaps=%d\n", szSearchFilter, bWorkshopOnly, m_vecAllMaps.Count() );

	// Remember the current selection
	char szCurrentValue[256] = { 0 };
	V_strncpy( szCurrentValue, pMapInfoObj->curValue, sizeof( szCurrentValue ) );

	pMapInfoObj->RemoveAndDeleteAllItems();

	// Add random map option
	pMapInfoObj->AddItem( new CScriptListItem( "#GameUI_RandomMap", "-1" ) );

	int iCount = m_vecAllMaps.Count();
	for ( int k = 0; k < iCount; ++k )
	{
		CreateServerMapItem item = m_vecAllMaps[k];
		const char *szMapName = item.mapName;
		bool bIsWorkshop = item.bIsWorkshopMap;

		// Filter by workshop if enabled
		if ( bWorkshopOnly && !bIsWorkshop )
			continue;

		// Filter by search text
		if ( szSearchFilter[0] != '\0' )
		{
			char szLowerMapName[MAX_PATH];
			V_strncpy( szLowerMapName, szMapName, sizeof( szLowerMapName ) );
			Q_strlower( szLowerMapName );

			if ( V_strstr( szLowerMapName, szSearchFilter ) == NULL )
				continue;
		}

		// Create display name - strip workshop/ prefix and .ugcXXX suffix for workshop maps
		char szDisplayName[MAX_PATH];
		V_strncpy( szDisplayName, szMapName, sizeof( szDisplayName ) );
		
		if ( bIsWorkshop )
		{
			// Strip "workshop/" prefix
			const char* pszStart = szMapName;
			if ( V_strnicmp( pszStart, "workshop/", 9 ) == 0 )
			{
				pszStart += 9;
			}
			V_strncpy( szDisplayName, pszStart, sizeof( szDisplayName ) );
			
			// Strip ".ugcXXX" suffix
			char* pszUgc = V_strstr( szDisplayName, ".ugc" );
			if ( pszUgc )
			{
				*pszUgc = '\0';
			}
		}

		// Store the actual index k (not filtered index) so we can look up workshop info
		pMapInfoObj->AddItem( new CScriptListItem( szDisplayName, CFmtStr( "%i", k ) ) );
	}

	// Update the ComboBox UI
	mpcontrol_t *pList = m_pList;
	while ( pList )
	{
		if ( pList->pScrObj == pMapInfoObj && pList->pControl )
		{
			ComboBox *pCombo = dynamic_cast<ComboBox*>( pList->pControl );
			if ( pCombo )
			{
				pCombo->RemoveAll();

				int iRow = 0;
				int iCurrentRow = 0;
				CScriptListItem *pListItem = pMapInfoObj->pListItems;
				while ( pListItem )
				{
					if ( !Q_stricmp( pListItem->szValue, szCurrentValue ) )
						iCurrentRow = iRow;

					pCombo->AddItem( pListItem->szItemText, NULL );
					pListItem = pListItem->pNext;
					iRow++;
				}

				pCombo->ActivateItemByRow( iCurrentRow );
			}
			break;
		}
		pList = pList->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when search text changes
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::OnTextChanged( vgui::Panel *panel )
{
	Warning( "OnTextChanged called, panel=%p, m_pMapSearchEntry=%p\n", panel, m_pMapSearchEntry );
	// Refresh map list if this came from the map search entry
	if ( panel == m_pMapSearchEntry )
	{
		Warning( "Refreshing map list from search\n" );
		RefreshMapList();
	}
	// Filter options if this came from the options search entry
	else if ( panel == m_pOptionsSearchEntry )
	{
		FilterOptions();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when workshop filter checkbox changes
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::OnCheckButtonChecked( int state )
{
	Warning( "OnCheckButtonChecked called, state=%d, m_pWorkshopFilterCheck=%p\n", state, m_pWorkshopFilterCheck );
	RefreshMapList();
}

//-----------------------------------------------------------------------------
// Purpose: Filter all options across all tabs based on search text
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::FilterOptions()
{
	// Get the search filter text
	char szSearchFilter[256] = { 0 };
	if ( m_pOptionsSearchEntry )
	{
		m_pOptionsSearchEntry->GetText( szSearchFilter, sizeof( szSearchFilter ) );
		Q_strlower( szSearchFilter );
	}

	bool bHasFilter = ( szSearchFilter[0] != '\0' );

	// If no filter, show everything in original order
	if ( !bHasFilter )
	{
		mpcontrol_t *pList = m_pList;
		while ( pList )
		{
			pList->SetVisible( true );
			pList = pList->next;
		}
		FOR_EACH_VEC( m_pPages, i )
		{
			if ( m_pPages[i] )
				m_pPages[i]->InvalidateLayout();
		}
		return;
	}

	// Clear all page lists and rebuild with matches first
	FOR_EACH_VEC( m_pPages, i )
	{
		if ( m_pPages[i] )
			m_pPages[i]->RemoveAll();
	}

	// First pass: Add matching items to their respective pages
	mpcontrol_t *pList = m_pList;
	while ( pList )
	{
		// Skip map-related controls as they have their own search
		if ( pList->pScrObj && 
			( !Q_stricmp( pList->pScrObj->cvarname, "cl_map" ) ||
			  !Q_stricmp( pList->pScrObj->cvarname, "cl_map_search" ) ||
			  !Q_stricmp( pList->pScrObj->cvarname, "cl_map_workshop_only" ) ) )
		{
			pList = pList->next;
			continue;
		}

		bool bMatches = false;

		if ( pList->pScrObj )
		{
			// Search in the prompt text
			char szLowerPrompt[256];
			V_strncpy( szLowerPrompt, pList->pScrObj->prompt, sizeof( szLowerPrompt ) );
			Q_strlower( szLowerPrompt );

			// Search in the cvar name
			char szLowerCvar[256];
			V_strncpy( szLowerCvar, pList->pScrObj->cvarname, sizeof( szLowerCvar ) );
			Q_strlower( szLowerCvar );

			// Search in the tooltip
			char szLowerTooltip[256] = { 0 };
			if ( pList->pScrObj->tooltip && pList->pScrObj->tooltip[0] )
			{
				V_strncpy( szLowerTooltip, pList->pScrObj->tooltip, sizeof( szLowerTooltip ) );
				Q_strlower( szLowerTooltip );
			}

			// Check if search text is found
			bMatches = ( V_strstr( szLowerPrompt, szSearchFilter ) != NULL ) ||
					   ( V_strstr( szLowerCvar, szSearchFilter ) != NULL ) ||
					   ( szLowerTooltip[0] != '\0' && V_strstr( szLowerTooltip, szSearchFilter ) != NULL );
		}

		if ( bMatches )
		{
			pList->SetVisible( true );
			if ( pList->pScrObj && pList->pScrObj->objParent )
			{
				PanelListPanel *pParent = dynamic_cast<PanelListPanel*>( pList->pScrObj->objParent );
				if ( pParent )
					pParent->AddItem( NULL, pList );
			}
		}

		pList = pList->next;
	}

	// Second pass: Add non-matching items (hidden)
	pList = m_pList;
	while ( pList )
	{
		// Skip map-related controls
		if ( pList->pScrObj && 
			( !Q_stricmp( pList->pScrObj->cvarname, "cl_map" ) ||
			  !Q_stricmp( pList->pScrObj->cvarname, "cl_map_search" ) ||
			  !Q_stricmp( pList->pScrObj->cvarname, "cl_map_workshop_only" ) ) )
		{
			pList = pList->next;
			continue;
		}

		bool bMatches = false;

		if ( pList->pScrObj )
		{
			char szLowerPrompt[256];
			V_strncpy( szLowerPrompt, pList->pScrObj->prompt, sizeof( szLowerPrompt ) );
			Q_strlower( szLowerPrompt );

			char szLowerCvar[256];
			V_strncpy( szLowerCvar, pList->pScrObj->cvarname, sizeof( szLowerCvar ) );
			Q_strlower( szLowerCvar );

			char szLowerTooltip[256] = { 0 };
			if ( pList->pScrObj->tooltip && pList->pScrObj->tooltip[0] )
			{
				V_strncpy( szLowerTooltip, pList->pScrObj->tooltip, sizeof( szLowerTooltip ) );
				Q_strlower( szLowerTooltip );
			}

			bMatches = ( V_strstr( szLowerPrompt, szSearchFilter ) != NULL ) ||
					   ( V_strstr( szLowerCvar, szSearchFilter ) != NULL ) ||
					   ( szLowerTooltip[0] != '\0' && V_strstr( szLowerTooltip, szSearchFilter ) != NULL );
		}

		if ( !bMatches )
		{
			pList->SetVisible( false );
			if ( pList->pScrObj && pList->pScrObj->objParent )
			{
				PanelListPanel *pParent = dynamic_cast<PanelListPanel*>( pList->pScrObj->objParent );
				if ( pParent )
					pParent->AddItem( NULL, pList );
			}
		}

		pList = pList->next;
	}

	// Invalidate all pages to refresh their layout
	FOR_EACH_VEC( m_pPages, i )
	{
		if ( m_pPages[i] )
			m_pPages[i]->InvalidateLayout();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::Deploy(void)
{
	SetVisible(true);
	MakePopup();
	MoveToFront();
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	TFModalStack()->PushModal(this);

	// Center it, keeping requested size
	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds(x, y, ww, wt);
	GetSize(wide, tall);
	SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

//-----------------------------------------------------------------------------
// Purpose: Updates the map preview image
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::OnThink() 
{
	// This is not as effecient as I wanted it to be.
	// Ideally I would want some event to hook onto, but I have no idea what i'm doing.

	// Check if map filters have changed
	if ( m_pMapSearchEntry || m_pWorkshopFilterCheck )
	{
		char szCurrentSearch[256] = { 0 };
		bool bCurrentWorkshopOnly = false;

		if ( m_pMapSearchEntry )
		{
			m_pMapSearchEntry->GetText( szCurrentSearch, sizeof( szCurrentSearch ) );
		}
		if ( m_pWorkshopFilterCheck )
		{
			bCurrentWorkshopOnly = m_pWorkshopFilterCheck->IsSelected();
		}

		// Check if either filter changed
		if ( Q_strcmp( szCurrentSearch, m_szLastSearchFilter ) != 0 || bCurrentWorkshopOnly != m_bLastWorkshopOnly )
		{
			V_strncpy( m_szLastSearchFilter, szCurrentSearch, sizeof( m_szLastSearchFilter ) );
			m_bLastWorkshopOnly = bCurrentWorkshopOnly;
			RefreshMapList();
		}
	}

	// Check if options search filter has changed
	if ( m_pOptionsSearchEntry )
	{
		char szCurrentOptionsSearch[256] = { 0 };
		m_pOptionsSearchEntry->GetText( szCurrentOptionsSearch, sizeof( szCurrentOptionsSearch ) );

		if ( Q_strcmp( szCurrentOptionsSearch, m_szLastOptionsSearchFilter ) != 0 )
		{
			V_strncpy( m_szLastOptionsSearchFilter, szCurrentOptionsSearch, sizeof( m_szLastOptionsSearchFilter ) );
			FilterOptions();
		}
	}

	//Msg("Think Enter\n");
	GatherCurrentValues(); // If this is not called, we would be reading old values.
	if ( m_pDescription )
	{
		//Msg("m_pDescription exists\n");
		CScriptObject* pMapInfoObj = m_pDescription->FindObject("cl_map");
		if (pMapInfoObj)
		{
			//Msg("pMapInfoObj exists\n");
			ImagePanel *pImagePanel = (ImagePanel*) FindChildByName( "map_preview_img", true );
			if (pImagePanel)
			{
				const char *szMapName = NULL;
				int nMapIndex = -1;
				
				// Find the current selection and its index
				CScriptListItem *pItem = pMapInfoObj->pListItems;
				int idx = 0;
				while ( pItem )
				{
					if (!Q_stricmp(pItem->szValue, pMapInfoObj->curValue))
					{
						szMapName = pItem->szItemText;
						// The value is "-1" for random or the index as string
						nMapIndex = atoi(pItem->szValue);
						break;
					}
					pItem = pItem->pNext;
					idx++;
				}
				
				//Msg("Current Selection: %s, index=%d\n", szMapName, nMapIndex);
				if( szMapName && nMapIndex >= 0 && nMapIndex < m_vecAllMaps.Count() )
				{
					CreateServerMapItem item = m_vecAllMaps[nMapIndex];
					bool bIsWorkshop = item.bIsWorkshopMap;
					PublishedFileId_t fileID = item.iMapFileId;
					
					if ( bIsWorkshop && fileID != 0 )
					{
						// Workshop map - check if we need to request preview
						if ( fileID != m_nLastDisplayedMapFileID )
						{
							m_nLastDisplayedMapFileID = fileID;
							Warning( "OnThink: Workshop map changed to fileID=%llu\n", fileID );
							
							// Check if we have a cached preview image (try both jpg and png)
							char szCachedJpg[MAX_PATH];
							char szCachedPng[MAX_PATH];
							char szCachedFullPath[MAX_PATH];
							V_snprintf( szCachedJpg, sizeof( szCachedJpg ), "%s/download/previews/workshop_preview_%llu.jpg", engine->GetGameDirectory(), fileID );
							V_snprintf( szCachedPng, sizeof( szCachedPng ), "%s/download/previews/workshop_preview_%llu.png", engine->GetGameDirectory(), fileID );
							
							const char* pCachedPath = NULL;
							if ( g_pFullFileSystem->FileExists( szCachedJpg ) )
							{
								pCachedPath = szCachedJpg;
							}
							else if ( g_pFullFileSystem->FileExists( szCachedPng ) )
							{
								pCachedPath = szCachedPng;
							}
							
							if ( pCachedPath )
							{
								// Load cached image as RGBA and create texture
								int nWidth = 0, nHeight = 0;
								ConversionErrorType result;
								unsigned char* pRGBA = ImgUtl_ReadImageAsRGBA( pCachedPath, nWidth, nHeight, result );
								
								if ( result == CE_SUCCESS && pRGBA && nWidth > 0 && nHeight > 0 )
								{
									// Set texture on our preview image wrapper
									m_pWorkshopPreviewImage->SetTextureRGBA( pRGBA, nWidth, nHeight );
									
									// Enable scaling and set our IImage on the panel
									pImagePanel->SetShouldScaleImage( true );
									pImagePanel->SetImage( m_pWorkshopPreviewImage );
									free( pRGBA );
								}
								else
								{
									// Failed to load cached image
									pImagePanel->SetImage( "maps/menu_thumb_default" );
								}
							}
							else
							{
								// Request the preview from workshop manager
								RequestWorkshopPreview( fileID );
								pImagePanel->SetImage( "maps/menu_thumb_default" );
							}
						}
					}
					else
					{
						// Regular map - use standard thumbnail
						m_nLastDisplayedMapFileID = 0;
						pImagePanel->SetShouldScaleImage( false );
						const char* szMapImage = CFmtStr("vgui/maps/menu_thumb_%s", szMapName);

						IMaterial *pMapMaterial = materials->FindMaterial( szMapImage, TEXTURE_GROUP_VGUI, false );
						if( pMapMaterial && !IsErrorMaterial( pMapMaterial ) )
						{
							pImagePanel->SetImage(CFmtStr("maps/menu_thumb_%s", szMapName));
						}
						else
						{ 
							pImagePanel->SetImage("maps/menu_thumb_default");
						}
					}
				}
				else if ( szMapName )
				{
					// Random map or invalid index - use default
					m_nLastDisplayedMapFileID = 0;
					pImagePanel->SetShouldScaleImage( false );
					const char* szMapImage = CFmtStr("vgui/maps/menu_thumb_%s", szMapName);

					IMaterial *pMapMaterial = materials->FindMaterial( szMapImage, TEXTURE_GROUP_VGUI, false );
					if( pMapMaterial && !IsErrorMaterial( pMapMaterial ) )
					{
						pImagePanel->SetImage(CFmtStr("maps/menu_thumb_%s", szMapName));
					}
					else
					{ 
						pImagePanel->SetImage("maps/menu_thumb_default");
					}
				}
			}
		}
	}
	//Msg("Think Exit\n");
	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: Request workshop preview image for a given file ID
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::RequestWorkshopPreview( PublishedFileId_t fileID )
{
	// Get the workshop item
	CCFWorkshopItem* pItem = CFWorkshop()->GetItem( fileID );
	if ( !pItem )
	{
		Warning( "RequestWorkshopPreview: Item %llu not found\n", fileID );
		return;
	}
	
	const char* pszURL = pItem->GetPreviewURL();
	if ( !pszURL || pszURL[0] == '\0' )
	{
		Warning( "RequestWorkshopPreview: No preview URL for item %llu\n", fileID );
		return;
	}
	
	ISteamHTTP* pHTTP = SteamHTTP();
	if ( !pHTTP )
	{
		Warning( "RequestWorkshopPreview: Steam HTTP not available\n" );
		return;
	}
	
	Warning( "RequestWorkshopPreview: Requesting preview for %llu from %s\n", fileID, pszURL );
	
	// Cancel any pending request
	if ( m_hPendingPreviewRequest != INVALID_HTTPREQUEST_HANDLE )
	{
		pHTTP->ReleaseHTTPRequest( m_hPendingPreviewRequest );
		m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
	}
	
	// Store which file ID we're requesting
	m_nCurrentPreviewFileID = fileID;
	
	// Create the HTTP request
	m_hPendingPreviewRequest = pHTTP->CreateHTTPRequest( k_EHTTPMethodGET, pszURL );
	if ( m_hPendingPreviewRequest == INVALID_HTTPREQUEST_HANDLE )
	{
		Warning( "RequestWorkshopPreview: Failed to create HTTP request\n" );
		return;
	}
	
	// Set timeout
	pHTTP->SetHTTPRequestNetworkActivityTimeout( m_hPendingPreviewRequest, 30 );
	
	// Send the request
	SteamAPICall_t hCall;
	if ( pHTTP->SendHTTPRequest( m_hPendingPreviewRequest, &hCall ) )
	{
		m_callbackHTTPPreview.Set( hCall, this, &CTFCreateServerDialog::Steam_OnPreviewImageReceived );
	}
	else
	{
		pHTTP->ReleaseHTTPRequest( m_hPendingPreviewRequest );
		m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
		Warning( "RequestWorkshopPreview: Failed to send HTTP request\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Callback when workshop preview image is downloaded
//-----------------------------------------------------------------------------
void CTFCreateServerDialog::Steam_OnPreviewImageReceived( HTTPRequestCompleted_t* pResult, bool bError )
{
	ISteamHTTP* pHTTP = SteamHTTP();
	if ( !pHTTP )
		return;
	
	Warning( "Steam_OnPreviewImageReceived: request=%u, error=%d, success=%d, status=%d\n",
		pResult->m_hRequest, bError ? 1 : 0, pResult->m_bRequestSuccessful ? 1 : 0, pResult->m_eStatusCode );
	
	// Check if this request is still valid
	if ( pResult->m_hRequest != m_hPendingPreviewRequest )
	{
		pHTTP->ReleaseHTTPRequest( pResult->m_hRequest );
		return;
	}
	
	m_hPendingPreviewRequest = INVALID_HTTPREQUEST_HANDLE;
	
	if ( bError || !pResult->m_bRequestSuccessful || pResult->m_eStatusCode != k_EHTTPStatusCode200OK )
	{
		Warning( "Steam_OnPreviewImageReceived: Request failed with status %d\n", pResult->m_eStatusCode );
		pHTTP->ReleaseHTTPRequest( pResult->m_hRequest );
		return;
	}
	
	// Get the body size
	uint32 unBodySize = 0;
	if ( !pHTTP->GetHTTPResponseBodySize( pResult->m_hRequest, &unBodySize ) || unBodySize == 0 )
	{
		Warning( "Steam_OnPreviewImageReceived: No response body\n" );
		pHTTP->ReleaseHTTPRequest( pResult->m_hRequest );
		return;
	}
	
	// Allocate buffer for the image data
	CUtlBuffer imageBuffer( 0, unBodySize, CUtlBuffer::READ_ONLY );
	imageBuffer.EnsureCapacity( unBodySize );
	
	if ( !pHTTP->GetHTTPResponseBodyData( pResult->m_hRequest, (uint8*)imageBuffer.Base(), unBodySize ) )
	{
		Warning( "Steam_OnPreviewImageReceived: Failed to get response body\n" );
		pHTTP->ReleaseHTTPRequest( pResult->m_hRequest );
		return;
	}
	
	imageBuffer.SeekPut( CUtlBuffer::SEEK_HEAD, unBodySize );
	pHTTP->ReleaseHTTPRequest( pResult->m_hRequest );
	
	// Determine image format from header bytes
	byte* pData = (byte*)imageBuffer.Base();
	const char* pszExt = ".jpg";
	
	// Look for PNG signature (89 50 4E 47)
	if ( unBodySize >= 8 && pData[0] == 0x89 && pData[1] == 'P' && pData[2] == 'N' && pData[3] == 'G' )
	{
		pszExt = ".png";
	}
	
	// Save to cache file
	char szCachePath[MAX_PATH];
	V_snprintf( szCachePath, sizeof( szCachePath ), "%s/download/previews", engine->GetGameDirectory() );
	g_pFullFileSystem->CreateDirHierarchy( szCachePath, "GAME" );
	
	char szFilePath[MAX_PATH];
	V_snprintf( szFilePath, sizeof( szFilePath ), "%s/download/previews/workshop_preview_%llu%s", 
		engine->GetGameDirectory(), m_nCurrentPreviewFileID, pszExt );
	
	FileHandle_t hFile = g_pFullFileSystem->Open( szFilePath, "wb", "GAME" );
	if ( hFile )
	{
		g_pFullFileSystem->Write( pData, unBodySize, hFile );
		g_pFullFileSystem->Close( hFile );
		
		Warning( "Steam_OnPreviewImageReceived: Saved preview to %s\n", szFilePath );
		
		// Update the image panel if this is still the selected map
		if ( m_nCurrentPreviewFileID == m_nLastDisplayedMapFileID )
		{
			// Load the image as RGBA
			int nWidth = 0, nHeight = 0;
			ConversionErrorType result;
			unsigned char* pRGBA = ImgUtl_ReadImageAsRGBA( szFilePath, nWidth, nHeight, result );
			
			if ( result == CE_SUCCESS && pRGBA && nWidth > 0 && nHeight > 0 )
			{
				Warning( "Steam_OnPreviewImageReceived: Loaded image %dx%d\n", nWidth, nHeight );
				
				// Set texture on our preview image wrapper
				m_pWorkshopPreviewImage->SetTextureRGBA( pRGBA, nWidth, nHeight );
				
				// Set the texture on the image panel
				ImagePanel *pImagePanel = (ImagePanel*) FindChildByName( "map_preview_img", true );
				if ( pImagePanel )
				{
					// Enable scaling and set our IImage on the panel
					pImagePanel->SetShouldScaleImage( true );
					pImagePanel->SetImage( m_pWorkshopPreviewImage );
				}
				
				// Free the RGBA data
				free( pRGBA );
			}
			else
			{
				Warning( "Steam_OnPreviewImageReceived: Failed to decode image (error %d)\n", result );
			}
		}
	}
	else
	{
		Warning( "Steam_OnPreviewImageReceived: Failed to save preview file\n" );
	}
}

#define MOD_CREDITS_DIR "cfg"
#define MOD_CREDITS_FILE MOD_CREDITS_DIR "/betterfortress_credits.txt"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTFModCreditsDialog::CTFModCreditsDialog(vgui::Panel* parent) : BaseClass(NULL, "TFModCreditsDialog")
{
	// Need to use the clientscheme (we're not parented to a clientscheme'd panel)
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFileEx(enginevgui->GetPanel(PANEL_CLIENTDLL), "resource/ClientScheme.res", "ClientScheme");
	SetScheme(scheme);
	SetProportional(true);

	m_pListPanel = new vgui::PanelListPanel(this, "PanelListPanel");
	m_pListPanel->SetFirstColumnWidth(0);

	m_pList = NULL;

	m_pToolTip = new CTFTextToolTip(this);
	m_pToolTipEmbeddedPanel = new vgui::EditablePanel(this, "TooltipPanel");
	m_pToolTipEmbeddedPanel->SetKeyBoardInputEnabled(false);
	m_pToolTipEmbeddedPanel->SetMouseInputEnabled(false);
	m_pToolTip->SetEmbeddedPanel(m_pToolTipEmbeddedPanel);
	m_pToolTip->SetTooltipDelay(0);

	m_pDescription = new CInfoDescription();

	KeyValuesAD pFileKV("CREDITS");
	if (!pFileKV->LoadFromFile(g_pFullFileSystem, MOD_CREDITS_FILE, "MOD"))
		return;

	for (KeyValues* pCurOption = pFileKV->GetFirstSubKey(); pCurOption; pCurOption = pCurOption->GetNextKey())
	{
		const char* pOptionName = pCurOption->GetName();
		CScriptObject* pObj = new CScriptObject();

		char type[64];
		const char* pParamType = pCurOption->GetString("type");
		Q_strncpy(type, pParamType, sizeof(type));
		Q_strncpy(pObj->cvarname, pOptionName, sizeof(pObj->cvarname));
		Q_strncpy(pObj->prompt, pCurOption->GetString("label", "Unnamed"), sizeof(pObj->prompt));
		Q_strncpy(pObj->tooltip, pCurOption->GetString("tooltip"), sizeof(pObj->tooltip));
		Q_strncpy(pObj->defValue, pCurOption->GetString("val"), sizeof(pObj->defValue));
		Q_strncpy(pObj->curValue, pObj->defValue, sizeof(pObj->curValue));
		pObj->fdefValue = atof(pObj->defValue);

		pObj->type = pObj->GetType(type);

		/*for (KeyValues* pCurParam = pCurOption->GetFirstSubKey(); pCurParam; pCurParam = pCurParam->GetNextKey())
		{
			const char* pParamName = pCurParam->GetName();
			if (!V_stricmp(pParamName, "options"))
			{
				for (KeyValues* pCurListItem = pCurParam->GetFirstSubKey(); pCurListItem; pCurListItem = pCurListItem->GetNextKey())
				{
					CScriptListItem* pItem;
					pItem = new CScriptListItem(pCurListItem->GetName(), pCurListItem->GetString());
					pObj->AddItem(pItem);
				}
			}

		}*/
		pObj->objParent = m_pListPanel;
		m_pDescription->AddObject(pObj);
	}


	//	m_pDescription->InitFromFile(MOD_CREDITS_FILE);
	// 	MoveToCenterOfScreen();
	// 	SetSizeable( false );
	// 	SetDeleteSelfOnClose( true );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTFModCreditsDialog::~CTFModCreditsDialog()
{
	delete m_pDescription;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	CreateControls();
	LoadControlSettings(RES_CREDITSMENU);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::ApplySettings(KeyValues* inResourceData)
{
	BaseClass::ApplySettings(inResourceData);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::OnClose()
{
	BaseClass::OnClose();

	TFModalStack()->PopModal(this);
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::OnCommand(const char* command)
{
	if (!stricmp(command, "Ok"))
	{
		OnClose();
		return;
	}
	else if (!stricmp(command, "Close"))
	{
		OnClose();
		return;
	}

	BaseClass::OnCommand(command);
}

void CTFModCreditsDialog::OnKeyCodeTyped(KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if (code == KEY_ESCAPE)
	{
		OnClose();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::OnKeyCodePressed(KeyCode code)
{
	// force ourselves to be closed if the escape key it pressed
	if (GetBaseButtonCode(code) == KEY_XBUTTON_B || GetBaseButtonCode(code) == STEAMCONTROLLER_B || GetBaseButtonCode(code) == STEAMCONTROLLER_START)
	{
		OnClose();
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::CreateControls()
{
	DestroyControls();

	// Go through desciption creating controls
	CScriptObject* pObj;

	pObj = m_pDescription->pObjList;
	mpcontrol_t* pCtrl;

	Button* pButton;
	Label* pBox;
	TextEntry* pEdit;
	ComboBox* pCombo;
	CCvarSlider* pSlider;
	CScriptListItem* pListItem;

	Panel* objParent = m_pListPanel;

	IScheme* pScheme = scheme()->GetIScheme(GetScheme());
	vgui::HFont hTextFont = pScheme->GetFont("HudFontSmallestBold", true);
	vgui::HFont hCreditFont = pScheme->GetFont("HudFontMediumSecondary", true);
	Color tanDark = pScheme->GetColor("TanDark", Color(255, 0, 0, 255));
	Color creditColor = Color(255, 255, 255, 255);

	while (pObj)
	{
		Msg("CVAR: %s LABEL: %s TOOLTIP: %s TYPE: %s VAL: %s PARENT: %s\n", pObj->cvarname, pObj->prompt, pObj->tooltip, GetTypeName(pObj->type), pObj->curValue, pObj->objParent->GetName());

		if (pObj->type == O_OBSOLETE)
		{
			pObj = pObj->pNext;
			continue;
		}

		pCtrl = new mpcontrol_t(objParent, "mpcontrol_t");
		pCtrl->type = pObj->type;

		// Force it to invalidate scheme now, so we can change color afterwards and have it persist
		pCtrl->InvalidateLayout(true, true);

		switch (pCtrl->type)
		{
		case O_BOOL:
			pBox = new Label(pCtrl, "CreditLabel", pObj->prompt);

			pCtrl->pControl = (Panel*)pBox;
			pBox->SetFont(hCreditFont);

			pBox->InvalidateLayout(true, true);

			pBox->SetFgColor(creditColor);
			break;
		case O_STRING:
		case O_NUMBER:
			pEdit = new TextEntry(pCtrl, "DescTextEntry");
			pEdit->InsertString(pObj->defValue);
			pCtrl->pControl = (Panel*)pEdit;
			pEdit->SetFont(hTextFont);

			pEdit->InvalidateLayout(true, true);
			pEdit->SetBgColor(Color(0, 0, 0, 255));
			break;
		case O_LIST:
		{
			pCombo = new ComboBox(pCtrl, "DescComboBox", 5, false);

			// track which row matches the current value
			int iRow = -1;
			int iCount = 0;
			pListItem = pObj->pListItems;
			while (pListItem)
			{
				if (iRow == -1 && !Q_stricmp(pListItem->szValue, pObj->curValue))
					iRow = iCount;

				pCombo->AddItem(pListItem->szItemText, NULL);
				pListItem = pListItem->pNext;
				++iCount;
			}


			pCombo->ActivateItemByRow(iRow);

			pCtrl->pControl = (Panel*)pCombo;
			pCombo->SetFont(hTextFont);
		}
		break;
		case O_SLIDER:
			pSlider = new CCvarSlider(pCtrl, "DescSlider", "Test", pObj->fMin, pObj->fMax, pObj->cvarname, false);
			pCtrl->pControl = (Panel*)pSlider;
			break;
		case O_BUTTON:
			pButton = new CExButton(pCtrl, "DescButton", pObj->prompt, this, pObj->defValue);
			pButton->SetFont(hTextFont);
			pCtrl->pControl = (Panel*)pButton;
			break;
		case O_CATEGORY:
			pCtrl->SetBorder(pScheme->GetBorder("OptionsCategoryBorder"));
			break;
		default:
			break;
		}

		if (pCtrl->type != O_BOOL && pCtrl->type != O_BUTTON)
		{
			pCtrl->pPrompt = new vgui::Label(pCtrl, "DescLabel", "");
			pCtrl->pPrompt->SetContentAlignment(vgui::Label::a_west);
			pCtrl->pPrompt->SetTextInset(5, 0);
			pCtrl->pPrompt->SetText(pObj->prompt);
			pCtrl->pPrompt->SetFont(hTextFont);

			pCtrl->pPrompt->InvalidateLayout(true, true);

			if (pCtrl->type == O_CATEGORY)
			{
				pCtrl->pPrompt->SetFont(pScheme->GetFont("HudFontSmallBold", true));
				pCtrl->pPrompt->SetFgColor(pScheme->GetColor("TanLight", Color(255, 0, 0, 255)));
			}
			else
			{
				pCtrl->pPrompt->SetFgColor(tanDark);
			}
		}

		pCtrl->pScrObj = pObj;

		switch (pCtrl->type)
		{
		case O_BOOL:
		case O_STRING:
		case O_NUMBER:
		case O_LIST:
		case O_BUTTON:
		case O_CATEGORY:
			pCtrl->SetSize(m_iControlW, m_iControlH);
			break;
		case O_SLIDER:
			pCtrl->SetSize(m_iSliderW, m_iSliderH);
			break;
		default:
			break;
		}

		// Hook up the tooltip, if the entry has one
		if (pObj->tooltip && pObj->tooltip[0])
		{
			if (pCtrl->pPrompt)
			{
				pCtrl->pPrompt->SetTooltip(m_pToolTip, pObj->tooltip);
			}
			else
			{
				pCtrl->SetTooltip(m_pToolTip, pObj->tooltip);
				pCtrl->pControl->SetTooltip(m_pToolTip, pObj->tooltip);
			}
		}

		m_pListPanel->AddItem(NULL, pCtrl);

		// Link it in
		if (!m_pList)
		{
			m_pList = pCtrl;
			pCtrl->next = NULL;
		}
		else
		{
			mpcontrol_t* p;
			p = m_pList;
			while (p)
			{
				if (!p->next)
				{
					p->next = pCtrl;
					pCtrl->next = NULL;
					break;
				}
				p = p->next;
			}
		}

		pObj = pObj->pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::DestroyControls()
{
	mpcontrol_t* p, * n;

	p = m_pList;
	while (p)
	{
		n = p->next;
		//
		if (p->pControl)
		{
			p->pControl->MarkForDeletion();
			p->pControl = NULL;
		}
		if (p->pPrompt)
		{
			p->pPrompt->MarkForDeletion();
			p->pPrompt = NULL;
		}
		delete p;
		p = n;
	}

	m_pList = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFModCreditsDialog::Deploy(void)
{
	SetVisible(true);
	MakePopup();
	MoveToFront();
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
	TFModalStack()->PushModal(this);

	// Center it, keeping requested size
	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds(x, y, ww, wt);
	GetSize(wide, tall);
	SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}