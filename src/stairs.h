#pragma once
#ifndef TERRAIN_H
#define TERRAIN_H

namespace MagicTower
{
    /* 
    CREATE TABLE stairs (
        id     INTEGER  PRIMARY KEY AUTOINCREMENT,
        type   INT (32),
        layers INT (32),
        x      INT (32),
        y      INT (32) 
    );
     */
    struct Stairs
    {
        std::size_t id;
        //1 up,2 down
        std::int32_t type;
        //[0,tower.HEIGHT)
        std::uint32_t layers;
        //[0,tower.LENGTH]
        std::uint32_t x;
        //[0,tower.WIDTH]
        std::uint32_t y;
    };
}

#endif
