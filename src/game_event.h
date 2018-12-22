#pragma once
#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#include <cstdint>

#include "tower.h"

namespace MagicTower
{
    struct GameEnvironment;

    std::vector<TowerGridLocation> find_path( GameEnvironment * game_object , TowerGridLocation goal );

    bool open_door( GameEnvironment * game_object , event_position_t position );

    bool check_grid_type( GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id );

    void set_tips( GameEnvironment * game_object , std::string tips_content );

    void set_grid_type( GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id = GRID_TYPE::FLOOR );

    bool move_hero( GameEnvironment * game_object , event_position_t position );

    std::int64_t get_combat_damage( GameEnvironment * game_object , std::uint32_t monster_id );

    //id start of 1
    bool battle( GameEnvironment * game_object , std::uint32_t monster_id );

    bool get_item( GameEnvironment * game_object , std::uint32_t item_id );

    bool change_floor( GameEnvironment * game_object , std::uint32_t stair_id );

    bool trigger_custom_event( GameEnvironment * game_object , std::string& event_string_ptr );

    //if can't move return false.
    bool trigger_collision_event( GameEnvironment * game_object );

    bool shopping( GameEnvironment * game_object , const char * commodity_json );

    void background_music_switch( GameEnvironment * game_object );

    void test_window_switch( GameEnvironment * game_object );

    void path_line_switch( GameEnvironment * game_object );

    void open_start_menu(  GameEnvironment * game_object );

    void open_game_menu_v2(  GameEnvironment * game_object );

    void close_game_menu_v2( GameEnvironment * game_object );

    void open_store_menu_v2( GameEnvironment * game_object );
    
    void close_store_menu_v2( GameEnvironment * game_object );

    void back_jump( GameEnvironment * game_object );
    
    void open_layer_jump( GameEnvironment * game_object );

    void close_layer_jump( GameEnvironment * game_object );

    void save_game( GameEnvironment * game_object , size_t save_id );

    void load_game( GameEnvironment * game_object , size_t save_id );

    void exit_game( GameEnvironment * game_object );

    void game_win( GameEnvironment * game_object );

    void game_lose( GameEnvironment * game_object );
}

#endif
