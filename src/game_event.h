#pragma once
#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#include <cstdint>

#include "tower.h"

namespace MagicTower
{
    struct GameEnvironment;
    typedef struct EventPosition
    {
        std::uint32_t floor;
        std::uint32_t x;
        std::uint32_t y;

    }position_t;

    inline bool operator==( const EventPosition& lhs , const EventPosition& rhs );
    inline bool operator!=( const EventPosition& lhs , const EventPosition& rhs );

    void scriptengines_register_eventfunc( GameEnvironment * game_object );

    std::vector<TowerGridLocation> find_path( GameEnvironment * game_object , TowerGridLocation goal );

    std::int64_t get_combat_damage( GameEnvironment * game_object , std::uint32_t monster_id );

    //id start of 1
    bool battle( GameEnvironment * game_object , std::uint32_t monster_id );

    //if can't move return false.
    bool move_hero( GameEnvironment * game_object , const position_t& new_pos );

    void open_start_menu( GameEnvironment * game_object );

    void open_game_menu(  GameEnvironment * game_object );

    void close_game_menu( GameEnvironment * game_object );

    void open_store_menu( GameEnvironment * game_object );
    
    void close_store_menu( GameEnvironment * game_object );
    
    void open_floor_jump( GameEnvironment * game_object );

    void close_floor_jump( GameEnvironment * game_object );

    void save_game( GameEnvironment * game_object , size_t save_id );

    void load_game( GameEnvironment * game_object , size_t save_id );

    void game_win( GameEnvironment * game_object );

    void game_lose( GameEnvironment * game_object );
}

#endif
