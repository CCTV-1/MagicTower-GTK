#pragma once
#ifndef ITEM_H
#define ITEM_H


#include <cstdint>
#include <string>

namespace MagicTower
{

enum ITEM_TYPE:std::uint32_t
{
    CHANGE_LEVEL = 0,
    CHANGE_LIFE,
    CHANGE_ATTACK,
    CHANGE_DEFENSE,
    CHANGE_GOLD,
    CHANGE_EXPERIENCE,
    CHANGE_YELLOW_KEY,
    CHANGE_BLUE_KEY,
    CHANGE_RED_KEY,
    CHANGE_ALL_KEY,
    CHANGE_ALL_ABILITY
};

/* 
CREATE TABLE item (
    id    INTEGER  PRIMARY KEY AUTOINCREMENT,
    name  TEXT,
    type  INT (32),
    value INT (32) 
);
 */
struct Item
{
    std::size_t item_id;
    std::string item_name;
    ITEM_TYPE ability_type;
    std::int32_t ability_value;
};

inline std::string dump_item_info( Item& item )
{
    std::string item_str;
    item_str += "名称:" + item.item_name;
    switch( item.ability_type )
    {
        case ITEM_TYPE::CHANGE_LEVEL:
            item_str += "\n物品能力: 等级";
            break;
        case ITEM_TYPE::CHANGE_LIFE:
            item_str += "\n物品能力: 生命值";
            break;
        case ITEM_TYPE::CHANGE_ATTACK:
            item_str += "\n物品能力: 攻击力";
            break;
        case ITEM_TYPE::CHANGE_DEFENSE:
            item_str += "\n物品能力: 防御力";
            break;
        case ITEM_TYPE::CHANGE_GOLD:
            item_str += "\n物品能力: 金币";
            break;
        case ITEM_TYPE::CHANGE_EXPERIENCE:
            item_str += "\n物品能力: 经验值";
            break;
        case ITEM_TYPE::CHANGE_YELLOW_KEY:
            item_str += "\n物品能力: 黄钥匙";
            break;
        case ITEM_TYPE::CHANGE_BLUE_KEY:
            item_str += "\n物品能力: 蓝钥匙";
            break;
        case ITEM_TYPE::CHANGE_RED_KEY:
            item_str += "\n物品能力: 红钥匙";
            break;
        case ITEM_TYPE::CHANGE_ALL_KEY:
            item_str += "\n物品能力: 所有钥匙各";
            break;
        case ITEM_TYPE::CHANGE_ALL_ABILITY:
            item_str += "\n物品能力: 全属性各加1/";
            break;
        default:
            item_str += "\n物品能力: 未知能力";
            break;
    }
    if ( item.ability_value >= 0 )
        item_str += "+" + std::to_string( item.ability_value );
    else
        item_str += std::to_string( item.ability_value );
    return item_str;
}

}

#endif
