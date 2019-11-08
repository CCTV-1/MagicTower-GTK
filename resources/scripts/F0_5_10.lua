local check_pos = 
{
    ["floor"] = 6,
    ["x"] = 5,
    ["y"] = 4
}
local move_pos =
{
    ["floor"] = 0,
    ["x"] = 5,
    ["y"] = 9
}

if get_grid_type(check_pos) ~= 1 then
    move_hero(move_pos)
    open_dialog("想临阵脱逃？不可能！")
end