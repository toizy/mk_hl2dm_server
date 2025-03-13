#include "cbase.h"
#include "mk_topmenu.h"
#include "mk_topmenu_builder.h"
#include "hl2mp_player.h"

ConVar mk_menu_next_btn( "mk_menu_next_btn", "> Next", FCVAR_NONE );
ConVar mk_menu_prev_btn( "mk_menu_prev_btn", "< Previous", FCVAR_NONE );
ConVar mk_menu_pagination_fmt( "mk_menu_pagination_fmt", "[ Page %d of %d ]", FCVAR_NONE );

constexpr auto MENU_CONTROL_BTN_HANDLER = "menu_control_btn_handler";
constexpr auto MENU_NEXT_PAGE_TOKEN = -1;
constexpr auto MENU_PREV_PAGE_TOKEN = -2;
constexpr auto MAX_COMMAND_LENGTH = 512;

//
// CMenuBuilder
//

CMenuBuilder::~CMenuBuilder()
{
	menuTemplate.items.Purge();
}

void CMenuBuilder::SetAlwaysOnTop( bool value )
{
	alwaysOnTop = value;
}

void CMenuBuilder::CreateMenu( const char* menu_name, const char* title, const char* msg, Color color, const int hold_time )
{
	UpdateMenuExpiry();

	menuName = menu_name;

	menuTemplate.items.Purge();

	menuTemplate.title = title;
	menuTemplate.description = msg;
	menuTemplate.textColor = color;
	menuTemplate.holdTime = hold_time;
	menuTemplate.endTime = gpGlobals->curtime + (float)hold_time;
	menuTemplate.selectedIndex = 0;
}

bool CMenuBuilder::AddMenuOption( const char* caption, const char* command )
{
	MenuItem item;
	item.title = caption;
	item.title.Trim();
	item.command = command;
	item.command.Trim();

	for ( auto it = menuTemplate.items.begin(); it != menuTemplate.items.end(); ++it )
	{
		if ( item.title.IsEqual_CaseInsensitive( (*it).title ) )
			return false;
	}

	menuTemplate.items.AddToTail( item );
	return true;
}

bool CMenuBuilder::IsValidClientIndex( const int clientIndex ) const
{
	return (clientIndex >= 0 && clientIndex < MAX_PLAYERS);
}

int CMenuBuilder::GetMaxItemsPerPage( const int clientIndex ) const
{
	return menuArray[clientIndex].items.Count() <= DEFAULT_ITEMS_PER_PAGE ? DEFAULT_ITEMS_PER_PAGE : MIN_ITEMS_PER_PAGE;
}

bool CMenuBuilder::ShouldUsePagination( const int clientIndex ) const
{
	return menuArray[clientIndex].items.Count() > DEFAULT_ITEMS_PER_PAGE;
}

int CMenuBuilder::GetCurrentMenuPage( CBasePlayer* pPlayer ) const
{
	int index = pPlayer->GetClientIndex();
	if ( !IsValidClientIndex( index ) )
		return 0;

	if ( !ShouldUsePagination( index ) )
		return 1;
	return (menuArray[index].selectedIndex / MIN_ITEMS_PER_PAGE) + 1;
}

int CMenuBuilder::GetLastMenuPage( CBasePlayer* pPlayer ) const
{
	int index = pPlayer->GetClientIndex();
	if ( !IsValidClientIndex( index ) )
		return 0;

	if ( !ShouldUsePagination( index ) )
		return 1;

	return (menuArray[index].items.Count() + MIN_ITEMS_PER_PAGE - 1) / MIN_ITEMS_PER_PAGE;
}

void CMenuBuilder::UpdateMenuExpiry()
{
	for ( auto& entry : menuArray )
	{
		if ( entry.endTime < gpGlobals->curtime )
		{
			entry = MenuEntry();
		}
	}
};

void CMenuBuilder::SendMenuToPlayer( const int clientIndex )
{
	if ( IsValidClientIndex( clientIndex ) == false )
		return;

	menuArray[clientIndex] = menuTemplate;

	if ( menuArray[clientIndex].items.Count() == 0 )
	{
		UTIL_LogPrintf( "[SendMenuToPlayer] Menu is empty for client %d\n", clientIndex );
		return;
	}

	RenderMenuForClient( clientIndex );
}

void CMenuBuilder::RenderMenuForClient( const int clientIndex )
{
	if ( !IsValidClientIndex( clientIndex ) )
		return;

	CBasePlayer* pPlayer = UTIL_PlayerByIndex( clientIndex + 1 );
	if ( pPlayer == NULL || pPlayer->IsBot() || pPlayer->IsFakeClient() || !pPlayer->IsConnected() )
		return;

	MenuEntry& entry = menuArray[clientIndex];

	const int maxItemsPerPage = GetMaxItemsPerPage( clientIndex );
	const int startIdx = entry.selectedIndex;
	const int endIdx = min( startIdx + maxItemsPerPage, entry.items.Count() );

	CUtlString description;

	if ( ShouldUsePagination( clientIndex ) )
	{
		int curPage = (startIdx / maxItemsPerPage) + 1;
		int totalPages = (entry.items.Count() + maxItemsPerPage - 1) / maxItemsPerPage;

		description.Format(
			"%s\n%s",
			CFmtStr( mk_menu_pagination_fmt.GetString(), curPage, totalPages ).Get(),
			entry.description.Get()
		);
	}
	else
	{
		description = entry.description;
	}

	CPluginMenu* menu = GetPluginMenu()->CreateMenu( entry.title, description, entry.textColor, entry.holdTime );

	if ( alwaysOnTop )
	{
		menu->BringToTop();
	}

	for ( int i = startIdx; i < endIdx; ++i )
	{
		CUtlString cmd;
		cmd.Format( "%s \"%s\"", MENU_CONTROL_BTN_HANDLER, entry.items[i].command.Get() );
		menu->AddMenuOption( entry.items[i].title, cmd.Get() );
	}

	if ( ShouldUsePagination( clientIndex ) )
	{
		auto AddButton = [&]( const char* text, int token ) {
			CUtlString cmd;
			cmd.Format( "%s %d %s", MENU_CONTROL_BTN_HANDLER, token, menuName.Get() );
			menu->AddMenuOption( text, cmd );
			};

		AddButton( mk_menu_next_btn.GetString(), MENU_NEXT_PAGE_TOKEN );
		AddButton( mk_menu_prev_btn.GetString(), MENU_PREV_PAGE_TOKEN );
	}

	menu->SendMenuToPlayer( pPlayer );
	delete menu;
}

void CMenuBuilder::BroadcastMenuToAllPlayers( void )
{
	if ( menuTemplate.items.Count() == 0 )
	{
		UTIL_LogPrintf( "[BroadcastMenuToAllPlayers] Menu is empty, cannot broadcast\n" );
		return;
	}

	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		SendMenuToPlayer( i );
	}
}

void CMenuBuilder::NavigatePage( int clientIndex, int direction )
{
	if ( !IsValidClientIndex( clientIndex ) )
		return;

	int itemCount = menuArray[clientIndex].items.Count();
	int maxPerPage = GetMaxItemsPerPage( clientIndex );

	int totalPages = (itemCount + maxPerPage - 1) / maxPerPage;
	int currentIndex = menuArray[clientIndex].selectedIndex;
	int currentPage = currentIndex / maxPerPage;

	int newPage = currentPage + direction;

	if ( newPage >= totalPages )
	{
		newPage = 0;
	}
	else if ( newPage < 0 )
	{
		newPage = totalPages - 1;
	}

	menuArray[clientIndex].selectedIndex = newPage * maxPerPage;

	RenderMenuForClient( clientIndex );
}

void CMenuBuilder::NavigateToNextPage( const int clientIndex )
{
	NavigatePage( clientIndex, 1 );
}

void CMenuBuilder::NavigateToPreviousPage( const int clientIndex )
{
	NavigatePage( clientIndex, -1 );
}

//
// CMenuManager
//

CMenuManager* GetTopMenuManager()
{
	static CMenuManager top_menu_manager;
	return &top_menu_manager;
}

unsigned int CMenuManager::GetMenuCount() const
{
	return menuPool.Count();
}

bool CMenuManager::ExecuteMenuAction( const char* name, const std::function<void( CMenuBuilder* )>& action )
{
	CMenuBuilder* menu = menuPool.FindElement( name, nullptr );
	if ( menu )
	{
		action( menu );
		return true;
	}
	return false;
}

CMenuManager::CMenuManager()
{
	menuPool.SetLessFunc( StringLessThan );
}

CMenuManager::~CMenuManager()
{
	ClearAll();
}

CMenuBuilder* CMenuManager::CreateMenu( const char* name, const char* title, const char* msg, Color color, int holdtime )
{
	return CreateMenu( name, title, msg, color, holdtime, 0 );
}

CMenuBuilder* CMenuManager::CreateMenu( const char* name, const char* title, const char* msg, Color color, int holdtime, int numCommands, ... )
{
	DeleteMenu( name );

	CMenuBuilder* menubuilder = new CMenuBuilder;
	menubuilder->CreateMenu( name, title, msg, color, holdtime );
	if ( numCommands > 0 )
	{
		va_list args;
		va_start( args, numCommands );
		for ( int i = 0; i < numCommands; ++i )
		{
			const char* caption = va_arg( args, const char* );
			const char* command = va_arg( args, const char* );
			menubuilder->AddMenuOption( caption, command );
		}
		va_end( args );
	}
	menuPool.Insert( name, menubuilder );
	return menubuilder;
}

void CMenuManager::DeleteMenu( const char* name )
{
	CMenuBuilder* menubuilder = menuPool.FindElement( name, nullptr );
	if ( menubuilder )
	{
		menuPool.Remove( name );
		delete menubuilder;
		menubuilder = nullptr;
	}
}

void CMenuManager::ClearAll()
{
	for ( unsigned int i = 0; i < menuPool.Count(); ++i )
	{
		delete menuPool[i];
	}
	menuPool.RemoveAll();
}

bool CMenuManager::SendMenuToPlayer( const char* name, int clientIndex )
{
	return ExecuteMenuAction( name, [clientIndex]( CMenuBuilder* menu )
		{
			menu->SendMenuToPlayer( clientIndex );
		} );
}

bool CMenuManager::BroadcastMenuToAllPlayers( const char* name, bool bringToTop )
{
	return ExecuteMenuAction( name, []( CMenuBuilder* menu )
		{
			menu->BroadcastMenuToAllPlayers();
		} );
}

bool CMenuManager::NavigateToNextPage( const char* name, int clientIndex ) const
{
	CMenuBuilder* menu = menuPool.FindElement( name, nullptr );
	if ( menu )
	{
		menu->NavigateToNextPage( clientIndex );
		return true;
	}
	return false;
}

bool CMenuManager::NavigateToPreviousPage( const char* name, int clientIndex ) const
{
	CMenuBuilder* menu = menuPool.FindElement( name, nullptr );
	if ( menu )
	{
		menu->NavigateToPreviousPage( clientIndex );
		return true;
	}
	return false;
}

static void cc_menu_control_btn_handler( const CCommand& args )
{
	if ( args.ArgC() < 2 )
	{
		UTIL_LogPrintf( "[menu_command_handler] Invalid command usage. Expected at least 2 arguments.\n" );
		return;
	}

	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	if ( !pPlayer )
		return;

	int clientIndex = pPlayer->GetClientIndex();
	int navigate = atoi( args[1] );
	const char* menu_name = args[2];

	if ( navigate == MENU_NEXT_PAGE_TOKEN )
	{
		GetTopMenuManager()->NavigateToNextPage( menu_name, clientIndex );
		return;
	}

	if ( navigate == MENU_PREV_PAGE_TOKEN )
	{
		GetTopMenuManager()->NavigateToPreviousPage( menu_name, clientIndex );
		return;
	}

	CUtlString cmd;
	for ( int i = 1; i < args.ArgC(); i++ )
	{
		if ( i > 1 )
			cmd.Append( " " );
		cmd.Append( args[i] );
	}

	UTIL_FakePlayerCommand( pPlayer->edict(), cmd.Get() );
}

static ConCommand menu_control_btn_handler( "menu_control_btn_handler", cc_menu_control_btn_handler, "", FCVAR_HIDDEN );

#ifdef DEBUG

//
// Usage example:
//

void cc_mk_playermodel( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	if ( !pPlayer )
		return;

	if ( args.ArgC() > 1 )
	{
		CHL2MP_Player* plr = ToHL2MPPlayer( pPlayer );
		if ( plr )
		{
			CBaseEntity::PrecacheModel( args[1] );

			plr->SetModel( args[1] );

			if ( args.ArgC() > 2 )
			{
				plr->m_nSkin = atoi( args[2] );
			}
		}
	}
}
static ConCommand mk_playermodel( "mk_playermodel", cc_mk_playermodel, "", FCVAR_HIDDEN );

void cc_test_menu( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	if ( !pPlayer )
		return;

	CMenuBuilder* mb = GetTopMenuManager()->CreateMenu( "test", "Title", "Description", COLOR_MK_RED, 100,
		21,
		"Play", "start_game",
		"Settings", "open_settings",
		"Quit", "exit_game",
		"1", "mk_playermodel \"models/combine_soldier.mdl\" 0",
		"2", "mk_playermodel \"models/combine_soldier.mdl\" 1",
		"3", "mk_playermodel \"models/humans/group02/female_03.mdl\"",
		"4", "mk_playermodel \"models/humans/group01/male_09.mdl\"",
		"5", "5",
		"6", "6",
		"7", "7",
		"8", "8",
		"9", "9",
		"10", "10",
		"11", "11",
		"12", "12",
		"13", "13",
		"14", "14",
		"15", "15",
		"16", "16",
		"17", "17",
		"18", "18"
	);

	mb->SetAlwaysOnTop( true );
	mb->SendMenuToPlayer( pPlayer->GetClientIndex() );
}
static ConCommand test_menu( "test_menu", cc_test_menu, "", FCVAR_HIDDEN );

void cc_test_menu_2( const CCommand& args )
{
	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	if ( !pPlayer )
		return;

	CMenuBuilder* mb = GetTopMenuManager()->CreateMenu( "test", "Title", "Description", COLOR_MK_RED, 100,
		3,
		"# 1", "test",
		"# 2", "test",
		"# 3", "test"
	);
	mb->BroadcastMenuToAllPlayers();
	GetTopMenuManager()->ClearAll();

}
static ConCommand test_menu_2( "test_menu_2", cc_test_menu_2, "", FCVAR_HIDDEN );

#endif
