#pragma once
#ifndef HERO_H
#define HERO_H

#include <cstdint>

namespace MagicTower
{
    
/* 
CREATE TABLE hero (
    id         INTEGER  PRIMARY KEY AUTOINCREMENT,
    layers     INT (32),
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
    red_key    INT (32) 
);
 */
extern "C" struct Hero
{
    std::uint32_t layers;
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
};

}

#endif
