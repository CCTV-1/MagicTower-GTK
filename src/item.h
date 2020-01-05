#pragma once
#ifndef ITEM_H
#define ITEM_H


#include <cstdint>
#include <string>

namespace MagicTower
{
    struct Item
    {
        bool needactive;
        std::string item_name;
        std::string item_detail;
    };
}

#endif
