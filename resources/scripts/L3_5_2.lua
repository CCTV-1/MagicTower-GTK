local check_pos = 
{
    ["floor"] = 3,
    ["x"] = 5,
    ["y"] = 3
}
local set_pos =
{
    ["floor"] = 3,
    ["x"] = 5,
    ["y"] = 2
}
if get_grid_type(check_pos) == 1 then
    set_grid_type(set_pos,1)
end