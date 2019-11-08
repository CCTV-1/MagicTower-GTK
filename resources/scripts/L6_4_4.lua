local check_pos = 
{
    ["floor"] = 6,
    ["x"] = 3,
    ["y"] = 4
}
local set_pos =
{
    ["floor"] = 6,
    ["x"] = 4,
    ["y"] = 4
}
if get_grid_type(check_pos) == 1 then
    set_grid_type(set_pos,1)
end