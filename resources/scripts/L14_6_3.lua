if get_flag("get_14_6_3_item") == nil then
    set_flag("get_14_6_3_item",0)
end
if get_flag("get_14_6_3_item") ~= 1 then
    local shop_menu = {}
    shop_menu["500金币换120防御"] = function()
        local hero_propertys=get_hero_property()
        if hero_propertys["gold"] >= 500 then
            hero_propertys["gold"] = hero_propertys["gold"] - 1
            hero_propertys["defense"] = hero_propertys["defense"] + 120
            set_hero_property(hero_propertys)
            set_flag("get_14_6_3_item",1)
            close_menu()
        end
    end
    open_menu(shop_menu)
else
    open_dialog("没有存货了")
end