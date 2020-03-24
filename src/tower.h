#pragma once
#ifndef TOWER_H
#define TOWER_H

#include <cstdint>

#include <string>

#include <vector>
#include <optional>
#include <map>
#include <memory>

namespace MagicTower
{
    enum class GRID_TYPE:std::uint32_t
    {
        BOUNDARY = 0,
        FLOOR,
        WALL,
        STAIRS,
        DOOR,
        NPC,
        MONSTER,
        ITEM,
        UNKNOWN
    };

    struct TowerGrid
    {
        //see enum GridType
        GRID_TYPE type;
        //see database table:monster,item,door... id column
        std::uint32_t id;
    };

    struct TowerGridLocation
    {
        //path search algorith call std::abs( x1 - x2 ).
        std::int64_t x, y;
    };

    /* CREATE TABLE towerfloor (
        id                  INTEGER  PRIMARY KEY AUTOINCREMENT,
        length              INT (32),
        width               INT (32),
        default_floorid     INT (32),
        name                TEXT,
        content             BLOB
    );
     */
    struct TowerFloor
    {
        std::uint32_t length;
        std::uint32_t width;
        //when grid type is GRID_TYPE::UNKNOWN display { GRID_TYPE::FLOOR , default_floorid },or some transparent grid(item,monster...),background image display { GRID_TYPE::FLOOR , default_floorid }
        std::uint32_t default_floorid;
        std::string name;
        std::optional<TowerGridLocation> teleport_point;
        std::vector<TowerGrid> content;
        //todo: field of vision
    };

    struct TowerMap
    {
        std::uint32_t MAX_LENGTH;
        std::uint32_t MAX_WIDTH;
        std::map<std::uint32_t,TowerFloor> map;

        TowerGrid get_grid( std::uint32_t floor_id , std::uint32_t x , std::uint32_t y )
        {
            if ( this->map.find( floor_id ) == this->map.end() )
            {
                return { GRID_TYPE::UNKNOWN , 0 };
            }
            TowerFloor& floor = this->map[floor_id];
            //starting from 0
            if ( ( x >= floor.length ) || ( y >= floor.width ) )
            {
                return { GRID_TYPE::UNKNOWN , 0 };
            }
            return floor.content[y*floor.length+x];
        }
        void set_grid( std::uint32_t floor_id , std::uint32_t x , std::uint32_t y , const TowerGrid& grid )
        {
            if ( this->map.find( floor_id ) == this->map.end() )
            {
                return ;
            }
            TowerFloor& floor = this->map[floor_id];
            //starting from 0
            if ( ( x >= floor.length ) || ( y >= floor.width ) )
            {
                return ;
            }
            floor.content[y*floor.length+x] = grid;
        }
    };
}

#endif
