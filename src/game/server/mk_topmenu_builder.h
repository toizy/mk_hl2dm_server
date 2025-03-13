#ifndef MK_TOPMENU_BUILDER_H
#define MK_TOPMENU_BUILDER_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

class CMenuBuilder
{
private:
	static constexpr int DEFAULT_ITEMS_PER_PAGE = 8;
	static constexpr int MIN_ITEMS_PER_PAGE = 6;

	struct MenuItem
	{
		CUtlString title;
		CUtlString command;
	};

	struct MenuEntry {
		CUtlVector<MenuItem> items;
		CUtlString	title{ "" };
		CUtlString	description{ "" };
		Color		textColor{ 255, 255, 255 };
		int			holdTime{ 0 };
		float		endTime{ 0 };
		int			selectedIndex{ 0 };

		MenuEntry() = default;

		MenuEntry& operator=( const MenuEntry& entry )
		{
			if ( this == &entry ) return *this;
			title = entry.title;
			description = entry.description;
			textColor = entry.textColor;
			holdTime = entry.holdTime;
			items = entry.items;
			selectedIndex = entry.selectedIndex;
			return *this;
		}
	};

	CUtlString				menuName{ "" };
	MenuEntry				menuTemplate;
	MenuEntry				menuArray[MAX_PLAYERS];
	bool					alwaysOnTop{ true };

	void RenderMenuForClient( const int clientIndex );
	bool IsValidClientIndex( const int clientIndex ) const;
	int GetMaxItemsPerPage( const int clientIndex ) const;
	bool ShouldUsePagination( const int clientIndex ) const;
	int GetCurrentMenuPage( CBasePlayer* pPlayer ) const;
	int GetLastMenuPage( CBasePlayer* pPlayer ) const;
	void UpdateMenuExpiry();
	void NavigatePage( int clientIndex, int direction );
public:
	CMenuBuilder() = default;
	~CMenuBuilder();;
	void SetAlwaysOnTop( bool value );
	void CreateMenu( const char* menu_name, const char* title, const char* msg, Color color, const int holdtime );
	bool AddMenuOption( const char* caption, const char* command );
	void NavigateToNextPage( const int clientIndex );
	void NavigateToPreviousPage( const int clientIndex );
	void SendMenuToPlayer( const int clientIndex );
	void BroadcastMenuToAllPlayers( void );
};

class CMenuManager
{
private:
	CUtlMap<const char*, CMenuBuilder*> menuPool;

	bool ExecuteMenuAction( const char* name, const std::function<void( CMenuBuilder* )>& action );
public:
	CMenuManager();
	~CMenuManager();
	CMenuBuilder* CreateMenu( const char* name, const char* title, const char* msg, Color color, int holdtime );
	CMenuBuilder* CreateMenu( const char* name, const char* title, const char* msg, Color color, int holdtime, int numCommands, ... );
	void DeleteMenu( const char* name );
	void ClearAll();
	bool SendMenuToPlayer( const char* name, int clientIndex );
	bool BroadcastMenuToAllPlayers( const char* name, bool bringToTop = false );
	bool NavigateToNextPage( const char* name, int clientIndex ) const;
	bool NavigateToPreviousPage( const char* name, int clientIndex ) const;
	unsigned int GetMenuCount() const;
};

CMenuManager* GetTopMenuManager();

#endif // MK_TOPMENU_BUILDER_H
