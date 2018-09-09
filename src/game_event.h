#pragma once
#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>

//ignore in gcc-8+ gtk library warning:unnecessary parentheses in declaration of '*'
#pragma GCC diagnostic ignored "-Wparentheses"
#include <gtk/gtk.h>
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
#define UI_DEFINE_RESOURCES_PATH "../resources/UI/magictower.ui"
#define IMAGE_RESOURCES_PATH "../resources/images/"
#define MUSIC_RESOURCES_PATH "../resources/music/"

namespace MagicTower
{
    extern int TOWER_GRID_SIZE;
    
    enum GAME_STATUS:std::uint32_t
    {
        NORMAL = 0,
        FIND_PATH,
        DIALOG,
        REVIEW_DETAIL,
        GAME_MENU,
        STORE_MENU,
        GAME_LOSE,
        GAME_WIN,
        UNKNOWN,
        STATUS_COUNT,
    };

    typedef std::vector< std::pair<std::string,std::function< void(void) > > > Menu_t;

    struct GameEnvironment
    {
        GtkBuilder * builder;
        std::shared_ptr<GdkPixbuf> info_frame;
        std::shared_ptr<PangoFontDescription> damage_font;
        std::shared_ptr<PangoFontDescription> info_font;
        std::shared_ptr<gchar> tips_content;
        std::map<std::string,std::shared_ptr<GdkPixbuf> > image_resource;
/* 
CREATE TABLE events (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    event_position TEXT,
    event_content  TEXT
);
*/
        std::map< MagicTower::event_position_t , std::string > custom_events;
        Menu_t& menu_items;
        std::size_t focus_item_id;
        MagicTower::Music& music;
        MagicTower::Hero hero;
        MagicTower::Tower towers;
        std::vector<MagicTower::Stairs>& stairs;
        std::vector<MagicTower::Monster>& monsters;
        std::vector<MagicTower::Item>& items;
        std::vector<MagicTower::TowerGridLocation> path;
        std::vector<MagicTower::Store> store_list;
        std::uint32_t game_status;
        bool draw_path;
        std::int32_t animation_value;
        int mouse_x;
        int mouse_y;
    };

    std::vector<TowerGridLocation> find_path( struct GameEnvironment * game_object , TowerGridLocation goal );

    bool open_door( struct GameEnvironment * game_object , event_position_t position );

    bool check_grid_type( struct GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id );

    void set_tips( struct GameEnvironment * game_object , std::shared_ptr<gchar> tips_content );

    void set_grid_type( struct GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id = GRID_TYPE::IS_FLOOR );

    bool move_hero( struct GameEnvironment * game_object , event_position_t position );

    std::int64_t get_combat_damage( struct GameEnvironment * game_object , std::uint32_t monster_id );

    //id start of 1
    bool battle( struct GameEnvironment * game_object , std::uint32_t monster_id );

    bool get_item( struct GameEnvironment * game_object , std::uint32_t item_id );

    bool change_floor( struct GameEnvironment * game_object , std::uint32_t stair_id );

    bool trigger_custom_event( struct GameEnvironment * game_object , std::string& event_string_ptr );

    //if can't move return false.
    bool trigger_collision_event( struct GameEnvironment * game_object );

    bool shopping( struct GameEnvironment * game_object , const char * commodity_json );

    void open_game_menu_v2(  struct GameEnvironment * game_object );

    void close_game_menu_v2( struct GameEnvironment * game_object );

    void open_game_menu( struct GameEnvironment * game_object );

    void close_game_menu( struct GameEnvironment * game_object );

    void open_store_menu_v2( struct GameEnvironment * game_object );
    
    void close_store_menu_v2( struct GameEnvironment * game_object );

    void open_store_menu( struct GameEnvironment * game_object );
    
    void close_store_menu( struct GameEnvironment * game_object );

    void save_game_status( struct GameEnvironment * game_object , size_t save_id );

    void load_game_status( struct GameEnvironment * game_object , size_t save_id );

    void game_win( struct GameEnvironment * game_object );

    void game_lose( struct GameEnvironment * game_object );

    void game_exit( struct GameEnvironment * game_object );
}

#endif
