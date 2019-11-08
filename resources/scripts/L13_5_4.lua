local check_pos = 
{
    ["floor"] = 13,
    ["x"] = 5,
    ["y"] = 5
}
local set_pos =
{
    ["floor"] = 13,
    ["x"] = 5,
    ["y"] = 4
}
if get_grid_type(check_pos) == 1 then
    set_grid_type(set_pos,1)
end