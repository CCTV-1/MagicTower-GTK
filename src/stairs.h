#pragma once
#ifndef TERRAIN_H
#define TERRAIN_H

namespace MagicTower
{
    struct Stairs
    {
        //1 up,2 down
        std::uint32_t type;
        //[0,tower.HEIGHT)
        std::uint32_t layers;
        //[0,tower.LENGTH]
        std::uint32_t x;
        //[0,tower.WIDTH]
        std::uint32_t y;
    };
}

#endif
