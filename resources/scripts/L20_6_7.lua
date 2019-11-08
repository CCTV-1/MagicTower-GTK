local check_pos = 
{
    ["floor"] = 20,
    ["x"] = 5,
    ["y"] = 1
}
local set_pos =
{
    ["floor"] = 20,
    ["x"] = 6,
    ["y"] = 7
}
if get_grid_type(check_pos) == 1 then
    set_grid_type(set_pos,1)
end