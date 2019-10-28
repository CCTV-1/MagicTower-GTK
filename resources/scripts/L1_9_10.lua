if get_flag("get_1_9_10_item") == nil then
    local hero_propertys=get_hero_property();
    hero_propertys["defense"] = hero_propertys["defense"] + 30;
    set_hero_property(hero_propertys);
    set_flag("get_1_9_10_item",1);
else
    open_dialog("我已经被掏空了")
end