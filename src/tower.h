#pragma once
#ifndef TOWER_H
#define TOWER_H

#include <cstdint>

#include <string>

#include <vector>
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

    struct TowerFloor
    {
        std::uint32_t length;
        std::uint32_t width;
        //when grid type is GRID_TYPE::UNKNOWN display { GRID_TYPE::FLOOR , default_floorid },or some transparent grid(item,monster...),background image display { GRID_TYPE::FLOOR , default_floorid }
        std::uint32_t default_floorid;
        std::string name;
        std::vector<TowerGrid> content;
    };

    struct TowerMap
    {
        std::map<std::uint32_t,TowerFloor> map;

        TowerGrid get_grid( std::uint32_t floor_id , std::uint32_t x , std::uint32_t y )
        {
            if ( this->map.find( floor_id ) == this->map.end() )
            {
                return { GRID_TYPE::UNKNOWN , 0 };
            }
            TowerFloor& floor = this->map[floor_id];
            if ( ( x > floor.length ) || ( y > floor.width ) )
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
            if ( ( x > floor.length ) || ( y > floor.width ) )
            {
                return ;
            }
            floor.content[y*floor.length+x] = grid;
        }
    };

    /* CREATE TABLE tower (
        id      INTEGER  PRIMARY KEY AUTOINCREMENT,
        length  INT (32),
        width   INT (32),
        height  INT (32),
        content BLOB
    );
     */
    struct Tower
    {
        std::uint32_t HEIGHT = 1;
        std::uint32_t LENGTH = 11;
        std::uint32_t WIDTH = 11;
        //struct TowerGrid [HEIGHT][LENGTH][WIDTH]
        std::vector<struct TowerGrid> maps;
    };

    inline struct TowerGrid& get_tower_grid( struct Tower& tower , std::uint32_t height = 0 , std::uint32_t length = 0 , std::uint32_t width = 0 )
    {
        //map[z][x][y]
        return tower.maps[ height*tower.LENGTH*tower.WIDTH + width*tower.WIDTH + length ];
    }

    inline void set_tower_grid( struct Tower& tower , struct TowerGrid& grid , std::uint32_t height = 0 , std::uint32_t length = 0 , std::uint32_t width = 0 )
    {
        std::uint32_t pos = height*tower.LENGTH*tower.WIDTH + width*tower.WIDTH + length;
        if ( ( tower.maps.size() < pos + 1 ) && ( pos < tower.maps.max_size() ) )
            tower.maps.resize( pos + 1 );
        tower.maps[ pos ] = grid;
    }
}

#endif
