can_open = get_flag("open_2layer_door")
if can_open == nil then
    open_dialog( "现在你可以打开第二层的宝石门了" , "快去吧" )
    set_flag("open_2layer_door",1)
end