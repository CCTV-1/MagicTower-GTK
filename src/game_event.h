#pragma once
#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#include <cstdint>

#include "tower.h"

namespace MagicTower
{
    struct GameStatus;
    typedef struct EventPosition
    {
        std::uint32_t floor;
        std::uint32_t x;
        std::uint32_t y;

    }position_t;

    inline bool operator==( const EventPosition& lhs , const EventPosition& rhs );
    inline bool operator!=( const EventPosition& lhs , const EventPosition& rhs );

    void scriptengines_register_eventfunc( GameStatus * game_status );

    std::vector<TowerGridLocation> find_path( GameStatus * game_status , TowerGridLocation goal , std::function<bool(std::uint32_t x , std::uint32_t y)> is_visible );

    std::int64_t get_combat_damage( GameStatus * game_status , std::uint32_t monster_id );

    //id start of 1
    bool battle( GameStatus * game_status , std::uint32_t monster_id );

    //if can't move return false.
    bool move_hero( GameStatus * game_status , const position_t& new_pos );

    bool use_item( GameStatus * game_status , std::uint32_t item_id );

    void open_start_menu( GameStatus * game_status );

    void open_game_menu(  GameStatus * game_status );

    void close_game_menu( GameStatus * game_status );

    void open_store_menu( GameStatus * game_status );
    
    void close_store_menu( GameStatus * game_status );
    
    void open_floor_jump( GameStatus * game_status );

    void close_floor_jump( GameStatus * game_status );

    void open_inventories_menu( GameStatus * game_status );

    void close_inventories_menu( GameStatus * game_status );

    void save_game( GameStatus * game_status , size_t save_id );

    void load_game( GameStatus * game_status , size_t save_id );

    void game_win( GameStatus * game_status );

    void game_lose( GameStatus * game_status );
}

#endif
