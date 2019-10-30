#pragma once
#ifndef STORE_H
#define STORE_H

#include <string>

namespace MagicTower
{
    struct Store
    {
        bool usability;
        std::string store_name;
        //commodity_detail , commodity_function
        std::map<std::string,std::string> commodities;
    };
}

#endif
