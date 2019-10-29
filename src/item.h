#pragma once
#ifndef ITEM_H
#define ITEM_H


#include <cstdint>
#include <string>

namespace MagicTower
{
    struct Item
    {
        std::string item_name;
        std::string item_detail;
        std::string item_func;
    };
}

#endif
