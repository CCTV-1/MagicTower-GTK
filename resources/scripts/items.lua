items=
{
    [1] =
    {
        ["item_name"] = "红宝石",
        ["item_detail"] = "攻击力加3",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["attack"] = hero_propertys["attack"] + 3
            set_hero_property(hero_propertys)
        end
    },
    [2] =
    {
        ["item_name"] = "蓝宝石",
        ["item_detail"] = "防御力加3",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["defense"] = hero_propertys["defense"] + 3
            set_hero_property(hero_propertys)
        end
    },
    [3] =
    {
        ["item_name"] = "黄钥匙",
        ["item_detail"] = "黄钥匙加1",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["yellow_key"] = hero_propertys["yellow_key"] + 1
            set_hero_property(hero_propertys)
        end
    },
    [4] =
    {
        ["item_name"] = "黄钥匙",
        ["item_detail"] = "蓝钥匙加1",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["blue_key"] = hero_propertys["blue_key"] + 1
            set_hero_property(hero_propertys)
        end
    },
    [5] =
    {
        ["item_name"] = "红钥匙",
        ["item_detail"] = "红钥匙加1",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["red_key"] = hero_propertys["red_key"] + 1
            set_hero_property(hero_propertys)
        end
    },
    [6] =
    {
        ["item_name"] = "小血瓶",
        ["item_detail"] = "生命值加200",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["life"] = hero_propertys["life"] + 200
            set_hero_property(hero_propertys)
        end
    },
    [7] =
    {
        ["item_name"] = "大血瓶",
        ["item_detail"] = "生命值加500",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["life"] = hero_propertys["life"] + 500
            set_hero_property(hero_propertys)
        end
    },
    [8] =
    {
        ["item_name"] = "铁剑",
        ["item_detail"] = "攻击力增加10",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["attack"] = hero_propertys["attack"] + 10
            set_hero_property(hero_propertys)
        end
    },
    [9] =
    {
        ["item_name"] = "铁盾",
        ["item_detail"] = "防御力增加10",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["defense"] = hero_propertys["defense"] + 10
            set_hero_property(hero_propertys)
        end
    },
    [10] =
    {
        ["item_name"] = "钥匙盒",
        ["item_detail"] = "所有钥匙各加1",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["yellow_key"] = hero_propertys["yellow_key"] + 1
            hero_propertys["blue_key"] = hero_propertys["blue_key"] + 1
            hero_propertys["red_key"] = hero_propertys["red_key"] + 1
            set_hero_property(hero_propertys)
        end
    },
    [11] =
    {
        ["item_name"] = "小金币",
        ["item_detail"] = "金币加100",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["gold"] = hero_propertys["gold"] + 100
            set_hero_property(hero_propertys)
        end
    },
    [12] =
    {
        ["item_name"] = "经验书",
        ["item_detail"] = "等级加1",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["level"] = hero_propertys["level"] + 1
            hero_propertys["life"] = hero_propertys["life"] + 1000
            hero_propertys["attack"] = hero_propertys["attack"] + 7
            hero_propertys["defense"] = hero_propertys["defense"] + 7
            set_hero_property(hero_propertys)
        end
    },
    [13] =
    {
        ["item_name"] = "钢剑",
        ["item_detail"] = "攻击力加70",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["attack"] = hero_propertys["attack"] + 70
            set_hero_property(hero_propertys)
        end
    },
    [14] =
    {
        ["item_name"] = "钢盾",
        ["item_detail"] = "防御力加70",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["defense"] = hero_propertys["defense"] + 70
            set_hero_property(hero_propertys)
        end
    },
    [15] =
    {
        ["item_name"] = "十字架",
        ["item_detail"] = "三围各加1/3",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["life"] = math.floor(hero_propertys["life"] + hero_propertys["life"]/3)
            hero_propertys["attack"] = math.floor(hero_propertys["attack"] + hero_propertys["attack"]/3)
            hero_propertys["defense"] = math.floor(hero_propertys["defense"] + hero_propertys["defense"]/3)
            set_hero_property(hero_propertys)
        end
    },
    [16] =
    {
        ["item_name"] = "大经验书",
        ["item_detail"] = "等级加3",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["level"] = hero_propertys["level"] + 3
            hero_propertys["life"] = hero_propertys["life"] + 3000
            hero_propertys["attack"] = hero_propertys["attack"] + 21
            hero_propertys["defense"] = hero_propertys["defense"] + 21
            set_hero_property(hero_propertys)
        end
    },
    [17] =
    {
        ["item_name"] = "圣水",
        ["item_detail"] = "生命值加30000",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["life"] = hero_propertys["life"] + 30000
            set_hero_property(hero_propertys)
        end
    },
    [18] =
    {
        ["item_name"] = "圣剑",
        ["item_detail"] = "攻击力加150",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["attack"] = hero_propertys["attack"] + 150
            set_hero_property(hero_propertys)
        end
    },
    [19] =
    {
        ["item_name"] = "圣盾",
        ["item_detail"] = "防御力加150",
        ["item_func"] = function()
            local hero_propertys = get_hero_property()
            hero_propertys["defense"] = hero_propertys["defense"] + 150
            set_hero_property(hero_propertys)
        end
    }
}