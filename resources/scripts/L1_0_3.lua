can_open = get_flag("open_2layer_door")
if can_open == nil then
    open_dialog( "now you can open 2 layer door" , "let go" )
    set_flag("open_2layer_door",1)
end