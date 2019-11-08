local check_pos = 
{
    ["floor"] = 1,
    ["x"] = 8,
    ["y"] = 6
}
local set_pos =
{
    ["floor"] = 1,
    ["x"] = 7,
    ["y"] = 7
}
if get_grid_type(check_pos) == 1 then
    set_grid_type(set_pos,1)
end