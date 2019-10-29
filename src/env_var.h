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
    enum GAME_STATUS:std::uint32_t
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
        GAME_LOSE,
        GAME_WIN,
        GAME_END,
        UNKNOWN,
    };

    typedef std::vector< std::pair<std::function< std::string(void) > , std::function< void(void) > > > Menu_t;

    struct GameEnvironment
    {
        GameEnvironment( std::vector<std::shared_ptr<const char> > music_list = {} );
        ~GameEnvironment();

        std::unique_ptr< lua_State , decltype( &lua_close ) > script_engines;
        std::deque<std::string> game_message;
        std::deque<std::string> tips_content;
        Menu_t menu_items;
        std::size_t focus_item_id;
        Music music;
        Hero hero;
        Tower towers;
        /* CREATE TABLE access_layers (
            layer INT (32) PRIMARY KEY AUTOINCREMENT,
        ); */
        std::map<std::uint32_t , bool> access_layer;
        /* CREATE TABLE jump_point (
            layer INT (32) PRIMARY KEY AUTOINCREMENT,
            x     INT (32),
            y     INT (32)
        ); */
        std::map<std::size_t , std::pair<std::size_t , std::size_t> > layers_jump;
        std::map<std::string , std::uint32_t> script_flags;
        std::vector<Stairs> stairs;
        std::vector<Monster> monsters;
        std::map<std::uint32_t,Item> items;
        std::vector<TowerGridLocation> path;
        std::vector<Store> store_list;
        enum GAME_STATUS game_status;
        bool draw_path;
    };
}

#endif
