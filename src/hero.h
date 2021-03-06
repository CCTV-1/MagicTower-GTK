#pragma once
#ifndef HERO_H
#define HERO_H

#include <cstdint>

#include <lua.hpp>

namespace MagicTower
{ 
    enum class DIRECTION:std::uint32_t
    {
        UP = 1,
        DOWN,
        LEFT,
        RIGHT
    };

    inline DIRECTION& operator++( DIRECTION& rhs )
    {
        if ( rhs < DIRECTION::RIGHT )
        {
            rhs = static_cast<DIRECTION>( static_cast<std::uint32_t>( rhs ) + 1 );
        }
        else
        {
            rhs = DIRECTION::UP;
        }
        
        return rhs;
    }

    inline DIRECTION operator++( DIRECTION& rhs , int )
    {
        DIRECTION temp = rhs;
        ++rhs;
        
        return temp;
    }

    /* 
    CREATE TABLE hero (
        id         INTEGER  PRIMARY KEY AUTOINCREMENT,
        floors     INT (32),
        x          INT (32),
        y          INT (32),
        level      INT (32),
        life       INT (32),
        attack     INT (32),
        defense    INT (32),
        gold       INT (32),
        experience INT (32),
        yellow_key INT (32),
        blue_key   INT (32),
        red_key    INT (32),
        direction  INT (32)
    );
     */
    struct Hero
    {
        std::uint32_t floors;
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t level;
        std::uint32_t life;
        std::uint32_t attack;
        std::uint32_t defense;
        std::uint32_t gold;
        std::uint32_t experience;
        std::uint32_t yellow_key;
        std::uint32_t blue_key;
        std::uint32_t red_key;
        enum DIRECTION direction;
    };

    //don't pop table,return hero
    inline Hero lua_gethero( lua_State * L , std::uint32_t index )
    {
        Hero hero;
        std::uint32_t top = lua_gettop( L );
        luaL_checktype( L , index , LUA_TTABLE );
    
        lua_getfield( L , index , "floors" );
        lua_getfield( L , index , "x" );
        lua_getfield( L , index , "y" );
        lua_getfield( L , index , "level" );
        lua_getfield( L , index , "life" );
        lua_getfield( L , index , "attack" );
        lua_getfield( L , index , "defense" );
        lua_getfield( L , index , "gold" );
        lua_getfield( L , index , "experience" );
        lua_getfield( L , index , "yellow_key" );
        lua_getfield( L , index , "blue_key" );
        lua_getfield( L , index , "red_key" );
        lua_getfield( L , index , "direction" );

        hero.floors = luaL_checkinteger( L , top + 1 );
        hero.x = luaL_checkinteger( L , top + 2 );
        hero.y = luaL_checkinteger( L ,top + 3 );
        hero.level = luaL_checkinteger( L , top + 4 );
        hero.life = luaL_checkinteger( L , top + 5 );
        hero.attack = luaL_checkinteger( L , top + 6 );
        hero.defense = luaL_checkinteger( L , top + 7 );
        hero.gold = luaL_checkinteger( L , top + 8 );
        hero.experience = luaL_checkinteger( L , top + 9 );
        hero.yellow_key = luaL_checkinteger( L , top + 10 );
        hero.blue_key = luaL_checkinteger( L , top + 11 );
        hero.red_key = luaL_checkinteger( L , top + 12 );
        hero.direction = static_cast<DIRECTION>( luaL_checkinteger( L , top + 13 ) );

        lua_pop( L , 13 );
        return hero;
    }

    //push a hero table to top
    inline void lua_pushhero( lua_State * L , const Hero& hero )
    {
        lua_newtable( L );
        std::uint32_t top = lua_gettop( L );
        lua_pushinteger( L , hero.floors );
        lua_setfield( L , top , "floors" );
        lua_pushinteger( L , hero.x );
        lua_setfield( L , top , "x" );
        lua_pushinteger( L , hero.y );
        lua_setfield( L , top , "y" );
        lua_pushinteger( L , hero.level );
        lua_setfield( L , top , "level" );
        lua_pushinteger( L , hero.life );
        lua_setfield( L , top , "life" );
        lua_pushinteger( L , hero.attack );
        lua_setfield( L , top , "attack" );
        lua_pushinteger( L , hero.defense );
        lua_setfield( L , top , "defense" );
        lua_pushinteger( L , hero.gold );
        lua_setfield( L , top , "gold" );
        lua_pushinteger( L , hero.experience );
        lua_setfield( L , top , "experience" );
        lua_pushinteger( L , hero.yellow_key );
        lua_setfield( L , top , "yellow_key" );
        lua_pushinteger( L , hero.blue_key );
        lua_setfield( L , top , "blue_key" );
        lua_pushinteger( L , hero.red_key );
        lua_setfield( L , top , "red_key" );
        lua_pushinteger( L , static_cast<lua_Integer>( hero.direction ) );
        lua_setfield( L , top , "direction" );
    }
}

#endif
