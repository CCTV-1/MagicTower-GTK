#pragma once
#ifndef ENV_VAR_H
#define ENV_VAR_H

#include <cstdint>

#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <vector>
#include <string>

#include "database.h"
#include "music.h"
#include "hero.h"
#include "item.h"
#include "monster.h"
#include "stairs.h"
#include "store.h"
#include "tower.h"

namespace MagicTower
{
    enum class GAME_STATE:std::uint32_t
    {
        NORMAL = 0,
        FIND_PATH,
        DIALOG,
        MESSAGE,
        REVIEW_DETAIL,
        GAME_MENU,
        START_MENU,
        STORE_MENU,
        JUMP_MENU,
        INVENTORIES_MENU,
        GAME_LOSE,
        GAME_WIN,
        GAME_END,
        UNKNOWN,
    };

    typedef std::vector< std::pair<std::function< std::string(void) > , std::function< void(void) > > > Menu_t;

    struct GameStatus
    {
        GameStatus( std::vector<std::string> music_list = {} );
        ~GameStatus();

        void initial_gamedata();

        bool draw_path;
        std::size_t focus_item_id;
        std::deque<std::string> game_message;
        std::deque<std::string> tips_content;
        std::map<std::uint32_t,bool> access_floor;
        std::map<std::uint32_t,Stairs> stairs;
        std::map<std::uint32_t,Monster> monsters;
        std::map<std::uint32_t,Item> items;
        std::map<std::uint32_t,Store> stores;
        std::map<std::uint32_t,std::uint32_t> inventories;
        std::map<std::string,std::uint32_t> script_flags;
        std::map<std::string,std::uint32_t> refmap;
        std::unique_ptr< lua_State , decltype( &lua_close ) > script_engines;
        std::vector<TowerGridLocation> path;
        Menu_t menu_items;
        MusicPlayer music;
        MusicPlayer soundeffect_player;
        Hero hero;
        TowerMap game_map;
        GAME_STATE state;
    };
}

#endif
