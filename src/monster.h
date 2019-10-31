#pragma once
#ifndef MONSTER_H
#define MONSTER_H

#include <cstdint>

#include <string>

namespace MagicTower
{
    enum ATTACK_TYPE:std::uint32_t
    {
        //if is normal,when the hero level is higher than the monster level,hero first attack
        FIRST_ATTACK = 0,
        NORMAL_ATTACK,
        LAST_ATTACK,
        DOUBLE_ATTACK,
        EXTRA_QUOTA_DAMAGE,
        EXTRA_PERCENT_DAMAG,
    };

    /* 
    CREATE TABLE monster (
        id         INTEGER  PRIMARY KEY AUTOINCREMENT,
        type       INT (32),
        type_value INT (32),
        name       TEXT,
        level      INT (32),
        life       INT (32),
        attack     INT (32),
        defense    INT (32),
        gold       INT (32),
        experience INT (32) 
    );
     */
    struct Monster
    {
        ATTACK_TYPE type;
        std::uint32_t type_value;
        std::string name;
        std::uint32_t level;
        std::uint32_t life;
        std::uint32_t attack;
        std::uint32_t defense;
        std::uint32_t gold;
        std::uint32_t experience;
    };

    inline std::string dump_monster_info( Monster& monster )
    {
        std::string monster_str;
        switch ( monster.type )
        {
            case ATTACK_TYPE::FIRST_ATTACK:
                monster_str += "攻击类型: 先攻";
                break;
            case ATTACK_TYPE::NORMAL_ATTACK:
                monster_str += "攻击类型: 标准";
                break;
            case ATTACK_TYPE::LAST_ATTACK:
                monster_str += "攻击类型: 后攻";
                break;
            case ATTACK_TYPE::DOUBLE_ATTACK:
                monster_str += "攻击类型: 连击";
                break;
            case ATTACK_TYPE::EXTRA_QUOTA_DAMAGE:
                monster_str += "攻击类型: 固伤";
                monster_str += std::to_string( monster.type_value );
                break;
            case ATTACK_TYPE::EXTRA_PERCENT_DAMAG:
                monster_str += "攻击类型: 固伤1/";
                monster_str += std::to_string( monster.type_value );
                break;
            default :
                monster_str += "未知类型";
                break;
        }
        monster_str += "\n名称:" + monster.name;
        monster_str += "\n等级:" + std::to_string( monster.level );
        monster_str += "\n生命:" + std::to_string( monster.life );
        monster_str += "\n攻击:" + std::to_string( monster.attack );
        monster_str += "\n防御:" + std::to_string( monster.defense );
        monster_str += "\n金钱:" + std::to_string( monster.gold );
        monster_str += "\n经验:" + std::to_string( monster.experience );
        return monster_str;
    }
}

#endif
