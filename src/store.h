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
        //commodity_detail vector --> GameEnvironment::regmap[commodity_detail] -> lua_ref return value
        //lua_rawgeti( L , LUA_REGISTRYINDEX , refvalue ) -> get lua func -> lua_call
        std::vector<std::string> commodities;
    };
}

#endif
