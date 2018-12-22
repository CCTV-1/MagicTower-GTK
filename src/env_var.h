#pragma once
#ifndef ENV_VAR_H
#define ENV_VAR_H

#include <cstdint>

#include <deque>
#include <functional>
#include <map>
#include <vector>
#include <string>

#include <jansson.h>

#include "database.h"
#include "music.h"
#include "hero.h"
#include "item.h"
#include "monster.h"
#include "stairs.h"
#include "store.h"
#include "tower.h"

#define DATABSE_RESOURCES_PATH "../resources/database/magictower.db"

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
        GameEnvironment():
            game_message( {} ),
            tips_content( {} ),
            custom_events( {} ),
            menu_items( {} ),
            focus_item_id( 0 ),
            music(),
            path( {} ),
            game_status( GAME_STATUS::NORMAL ),
            draw_path( true )
        {
            DataBase db( DATABSE_RESOURCES_PATH );
            this->towers = db.get_tower_info( 0 );
            this->hero = db.get_hero_info( 0 );
            this->stairs = db.get_stairs_list();
            this->store_list = db.get_store_list();
            this->monsters = db.get_monster_list();
            this->items = db.get_item_list();
            this->custom_events = db.get_custom_events();
            this->access_layer = db.get_access_layers();
            this->layers_jump = db.get_jump_map();
        }
        GameEnvironment( std::vector<std::shared_ptr<const char> > music_list ):
            game_message( {} ),
            tips_content( {} ),
            custom_events( {} ),
            menu_items( {} ),
            focus_item_id( 0 ),
            music( music_list ),
            path( {} ),
            game_status( GAME_STATUS::NORMAL ),
            draw_path( true )
        {
            DataBase db( DATABSE_RESOURCES_PATH );
            this->towers = db.get_tower_info( 0 );
            this->hero = db.get_hero_info( 0 );
            this->stairs = db.get_stairs_list();
            this->store_list = db.get_store_list();
            this->monsters = db.get_monster_list();
            this->items = db.get_item_list();
            this->custom_events = db.get_custom_events();
            this->access_layer = db.get_access_layers();
            this->layers_jump = db.get_jump_map();
        }

        ~GameEnvironment()
        {
            ;
        }
        std::deque<std::string> game_message;
        std::deque<std::string> tips_content;
/* 
CREATE TABLE events (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    event_position TEXT,
    event_content  TEXT
);
*/
        std::map<event_position_t , std::string> custom_events;
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
        std::vector<Stairs> stairs;
        std::vector<Monster> monsters;
        std::vector<Item> items;
        std::vector<TowerGridLocation> path;
        std::vector<Store> store_list;
        enum GAME_STATUS game_status;
        bool draw_path;
    };
}

#endif
