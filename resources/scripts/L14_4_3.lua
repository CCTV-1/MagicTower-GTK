if get_flag("get_14_4_3_item") == nil then
    set_flag("get_14_4_3_item",0)
end
if get_flag("get_14_4_3_item") ~= 1 then
    local shop_menu = {}
    shop_menu["500经验值换120攻击"]="local hero_propertys=get_hero_property();if hero_propertys[\"experience\"] >= 500 then hero_propertys[\"experience\"] = hero_propertys[\"experience\"] - 1;hero_propertys[\"attack\"] = hero_propertys[\"attack\"] + 120;set_hero_property(hero_propertys);set_flag(\"get_14_4_3_item\",1);close_menu();end"
    open_menu(shop_menu)
else
    open_dialog("没有存货了")
end