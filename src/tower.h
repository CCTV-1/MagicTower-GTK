#pragma once
#ifndef TOWER_H
#define TOWER_H

#include <cstdint>

#include <string>

#include <vector>
#include <memory>

namespace MagicTower
{

enum GRID_TYPE:std::uint32_t
{
    IS_BOUNDARY = 0,
    IS_FLOOR,
    IS_WALL,
    IS_STAIRS,
    IS_DOOR,
    IS_NPC,
    IS_MONSTER,
    IS_ITEM
};

#ifdef _cplusplus
extern "C"
#endif
struct TowerGrid
{
    //see enum GridType
    std::uint32_t type;
    //see database table:monster,item,door... id column
    std::uint32_t id;
};

//x,y,layer --> 1,5,10
typedef std::tuple<std::uint32_t,std::uint32_t,std::uint32_t> event_position_t;

struct TowerGridLocation
{
    //path search algorith call std::abs( x1 - x2 ).
	std::int64_t x, y;
};

/* CREATE TABLE tower (
    id      INTEGER  PRIMARY KEY AUTOINCREMENT,
    length  INT (32),
    width   INT (32),
    height  INT (32),
    content BLOB
);
 */
#ifdef _cplusplus
extern "C"
#endif
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

inline std::string dump_tower_map( struct Tower& tower )
{
    std::string map_str;
    map_str += "{";
    for ( std::uint32_t z = 0 ; z < tower.HEIGHT ; z++ )
    {
        map_str += "\n";
        for( std::uint32_t y = 0 ; y < tower.LENGTH ; y++ )
        {
            for ( std::uint32_t x = 0 ; x < tower.WIDTH ; x++ )
            {
                auto grid = get_tower_grid( tower, z , x , y );
                map_str += " { " + std::to_string( grid.type ) + " , " + std::to_string( grid.id ) + " } ,";
            }
            map_str += "\n";
        }
        map_str += "\n";
    }
    map_str += "}";
    return map_str;
}

}

#endif
