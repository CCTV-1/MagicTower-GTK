stores =
{
    [1] =
    {
        ["usability"] = false,
        ["store_name"] = "初级金币商店",
        ["commodities"] = 
        {
            ["25金币购买800生命值"] = function()
            local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 25 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 25
                    hero_propertys["life"] = hero_propertys["life"] + 800
                    set_hero_property(hero_propertys)
                end
            end,
            ["25金币购买4攻击"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 25 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 25
                    hero_propertys["attack"] = hero_propertys["attack"] + 4
                    set_hero_property(hero_propertys)
                end
            end,
            ["25金币购买4防御"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 25 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 25
                    hero_propertys["defense"] = hero_propertys["defense"] + 4
                    set_hero_property(hero_propertys)
                end
            end
        }
    },
    [2] =
    {
        ["usability"] = false,
        ["store_name"] = "初级经验商店",
        ["commodities"] =
        {
            ["100经验升1级"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["experience"] >= 100 then
                    hero_propertys["experience"] = hero_propertys["experience"] - 100
                    hero_propertys["level"] = hero_propertys["level"] + 1
                    hero_propertys["life"] = hero_propertys["life"] + 1000
                    hero_propertys["attack"] = hero_propertys["attack"] + 7
                    hero_propertys["defense"] = hero_propertys["defense"] + 7
                    set_hero_property(hero_propertys)
                end
            end,
            ["30经验加5攻击"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["experience"] >= 30 then
                    hero_propertys["experience"] = hero_propertys["experience"] - 30
                    hero_propertys["attack"] = hero_propertys["attack"] + 5
                    set_hero_property(hero_propertys)
                end
            end,
            ["30经验加5防御"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["experience"] >= 30 then
                    hero_propertys["experience"] = hero_propertys["experience"] - 30
                    hero_propertys["defense"] = hero_propertys["defense"] + 5
                    set_hero_property(hero_propertys)
                end
            end
        }
    },
    [3] =
    {
        ["usability"] = false,
        ["store_name"] = "钥匙出售商人",
        ["commodities"] =
        {
            ["10金币买一把黄钥匙"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 10 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 10
                    hero_propertys["yellow_key"] = hero_propertys["yellow_key"] + 1
                    set_hero_property(hero_propertys)
                end
            end,
            ["50金币买一把蓝钥匙"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 50 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 50
                    hero_propertys["blue_key"] = hero_propertys["blue_key"] + 1
                    set_hero_property(hero_propertys)
                end
            end,
            ["100金币买一把红钥匙"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 100 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 100
                    hero_propertys["red_key"] = hero_propertys["red_key"] + 1
                    set_hero_property(hero_propertys)
                end
            end
        }
    },
    [4] =
    {
        ["usability"] = false,
        ["store_name"] = "高级金币商店",
        ["commodities"] =
        {
            ["100金币购买4000生命值"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 100 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 100
                    hero_propertys["life"] = hero_propertys["life"] + 4000
                    set_hero_property(hero_propertys)
                end
            end,
            ["100金币购买20攻击"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 100 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 100
                    hero_propertys["attack"] = hero_propertys["attack"] + 20
                    set_hero_property(hero_propertys)
                end
            end,
            ["100金币购买20防御"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["gold"] >= 100 then
                    hero_propertys["gold"] = hero_propertys["gold"] - 100
                    hero_propertys["defense"] = hero_propertys["defense"] + 20
                    set_hero_property(hero_propertys)
                end
            end
        }
    },
    [5] =
    {
        ["usability"] = false,
        ["store_name"] = "高级经验商店",
        ["commodities"] =
        {
            ["270经验升3级"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["experience"] >= 270 then
                    hero_propertys["experience"] = hero_propertys["experience"] - 270
                    hero_propertys["level"] = hero_propertys["level"] + 3
                    hero_propertys["life"] = hero_propertys["life"] + 3000
                    hero_propertys["attack"] = hero_propertys["attack"] + 20
                    hero_propertys["defense"] = hero_propertys["defense"] + 20
                    set_hero_property(hero_propertys)
                end
            end,
            ["95经验加17攻击"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["experience"] >= 95 then
                    hero_propertys["experience"] = hero_propertys["experience"] - 95
                    hero_propertys["attack"] = hero_propertys["attack"] + 17
                    set_hero_property(hero_propertys)
                end
            end,
            ["95经验加17防御"] = function()
                local hero_propertys = get_hero_property();
                if hero_propertys["experience"] >= 95 then
                    hero_propertys["experience"] = hero_propertys["experience"] - 95
                    hero_propertys["defense"] = hero_propertys["defense"] + 17
                    set_hero_property(hero_propertys)
                end
            end
        }
    },
    [6] =
    {
        ["usability"] = false,
        ["store_name"] = "钥匙回收商人",
        ["commodities"] =
        {
            ["一把黄钥匙7金币回收"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["yellow_key"] >= 1 then
                    hero_propertys["yellow_key"] = hero_propertys["yellow_key"] - 1
                    hero_propertys["gold"] = hero_propertys["gold"] + 7
                    set_hero_property(hero_propertys)
                end
            end,
            ["一把蓝钥匙35金币回收"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["blue_key"] >= 1 then
                    hero_propertys["blue_key"] = hero_propertys["blue_key"] - 1
                    hero_propertys["gold"] = hero_propertys["gold"] + 35
                    set_hero_property(hero_propertys)
                end
            end,
            ["一把红钥匙70金币回收"] = function()
                local hero_propertys = get_hero_property()
                if hero_propertys["red_key"] >= 1 then
                    hero_propertys["red_key"] = hero_propertys["red_key"] - 1
                    hero_propertys["gold"] = hero_propertys["gold"] + 70
                    set_hero_property(hero_propertys)
                end
            end
        }
    }
}