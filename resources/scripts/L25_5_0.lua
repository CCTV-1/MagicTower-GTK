local check_pos = 
{
    ["floor"] = 25,
    ["x"] = 5,
    ["y"] = 0
}
if get_grid_type(check_pos) == 1 then
    game_win()
else
    game_lose()
end