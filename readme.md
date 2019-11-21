# MagicTower-GTK

## Dependency Library:
-  ```GTKMM3 3.22```+ 
-  ```GStreamer 1.12```+
-  ```SQLITE3 3.21```+
-  ```lua 5.3```+

## Language: 
- ```C++11```+

## Game Script

**if no special statement,this chapter all 'number' is lua_Integer in C API type**

### Table Format
```lua
hero =                  --see hero.h
{
    ["floors"] = 1,
    ["x"] = 1,
    ["y"] = 1,
    ["level"] = 1,
    ["life"] = 1,
    ["attack"] = 1,
    ["defense"] = 1,
    ["gold"] = 1,
    ["experience"] = 1,
    ["yellow_key"] = 1,
    ["blue_key"] = 1,
    ["red_key"] = 1
}

menu_detail =
{
    -- 'level up' is function reference map key -> game call map["level up"] get func(by lua_ref lua_rewgeti lua_call).
    -- if the string already exists in the map,simple skip save function reference,don't replace old function reference.
    -- the function,click menu item to trigger call.
    ["level up"] = function()
        local hero_propertys = get_hero_property()
        hero_propertys["level"] = hero_propertys["level"] + 1
        hero_propertys["life"] = hero_propertys["life"] + 1000
        hero_propertys["attack"] = hero_propertys["attack"] + 7
        hero_propertys["defense"] = hero_propertys["defense"] + 7
        set_hero_property(hero_propertys)
        close_menu()
    end
    ...
}

position =
{
    ["floor"] = 1,
    ["x"] = 1,
    ["y"] = 1
}

grid = 
{
    ["type"] = 1,   --see tower.h GRID_TYPE
    ["id"] = 1,     --$(type),$(id) determine which image to game use
}
```

### Export functions

```lua

--return a number(is lua_Number in C API type) 
function get_volume( void )

--no return,the argument number is lua_Number in C API type
function set_volume( number )

--no return
--mode value see music.h enum PLAY_MODE
function set_playmode( number )

--no return
--argument:any number string(Lua c api exist limits)
--uri like: file:///C:/MSYS2/home/user/1.mp3
function set_playlist( string uri , ... )

--no return
function play_next( void )

--no return
function play_pause( void )

--no return
function play_resume( void )

--no return
function set_tips( string tips_content )

--no return
--id set to the floor default floorid
function set_grid_type( table position , number type_id )

--return a number
--if don't care grid id,use this
function get_grid_type( table position )

--no return
function set_grid( table position , table grid )

--return a grid table
function get_grid( table position )

--return a table:
function get_hero_property( void )

--no return
function set_hero_property( table hero )

--if flag not exist,reutn nil.if exist,return a number
function get_flag( string flag_name )

--no return
function set_flag( string flag_name , number flag_value )

--no return
--argument:any number string(Lua c api exist limits)
function open_dialog( string dialog_content , ... )

--no return
function open_menu( table menu_detail )

--no return
function close_menu()

--no return
function get_item( number item_id )

--no return
function unlock_store( number store_id )

--no return
function lock_store( number store_id )

--no return
function move_hero( table position )

--no return
function game_win()

--no return
function game_lose()
```

### keeped name
- Z2FtZV9vYmplY3QK
- items
- monsters
- stores
- stairs
- hero_property
- floorjump
- gamemap

**forbid modify Z2FtZV9vYmplY3QK from game script**

### keeped flag
- stores_$(stores_id)
- floor_$(floor_id)

### script name rule

- F_$(floor)\_$(x)\_$(y).lua  --> call for each move to new position(before the built-in events)

- L_$(floor)\_$(x)\_$(y).lua  --> call for each move to new position(after the built-in event)

- items.lua --> defines game items function,the contents as follows:

```lua
items =
{
    [item_id] =                 --item_id is a number
    {
        ["item_name"] = ""      --detail dialog display text
        ["item_detail"] = "",   --detail dialog display text,item_detail is function reference map key -> game call map[item_detail] get func(by lua_ref lua_rewgeti lua_call).
        -- if the string already exists in the map,simple skip save function reference,don't replace old function reference.
        ["item_func"] = function()
            ;                   --get item trigger call this function,0 argument,0 return value.
        end
    },
    ...
}
```

- hero.lua --> defines game hero initial property,the contents as follows:
```lua
hero_property=
{
    ["floors"] = 0,
    ["x"] = 5,
    ["y"] = 9,
    ["level"] = 1,
    ["life"] = 1000,
    ["attack"] = 10,
    ["defense"] = 10,
    ["gold"] = 0,
    ["experience"] = 0,
    ["yellow_key"] = 0,
    ["blue_key"] = 1,
    ["red_key"] = 1
}
```

- monsters.lua --> defines game monsters property,the contents as follows:
```lua
monsters =
{
    [monster_id] = --monster_id is a number.
    {
        ["attack_type"] = 1,
        ["type_value"] = 0,
        ["name"] = "",
        ["level"] = 1,
        ["life"] = 50,
        ["attack"] = 20,
        ["defense"] = 1,
        ["gold"] = 1,
        ["experience"] = 1
    },
    ...
}
```

- stores.lua --> defines game stores function,the contents as follows:
```lua
stores=
{
    [store_id] =                    --store_id is a number.
    {
        ["usability"] = false,      --if is true,or call unlock_store,set flag:store_id to non nul value,else to nil value.
        ["store_name"] = "",        --store menu display text
        ["commodities"] =
        {
            -- commodity_detail is a string,store menu display text
            -- commodity_detail is function reference map key -> game call map[item_detail] get func(by lua_ref lua_rewgeti lua_call).
            -- if the string already exists in the map,simple skip save function reference,don't replace old function reference.
            [commodity_detail] = function()
                ;                   -- buy the commodity,trigger call this function.0 argument,0 return value.
            end,
            ...
        }
    },
    ...
}
```

- stairs.lua --> defines game stairs property,the contents as follows:
```lua
stairs=
{
    [stair_id] =                --stair_id is a number.
    {
        ["image_type"] = 1,     --the number -> resources/images/stairs$(image_type).png
        ["floor"] = 1,          --number
        ["x"] = 1,              --number,enter the stair hero go to ( floor , x , y )
        ["y"] = 1,              --number
    },
    ...
}
```

- jumper.lua --> defines game floor jump property,the contents as follows:
```lua
floorjump =
{
    [0] =           --jump to 0 floor,position:(x,y) = (5,10)
    {               --when a floor does not exist in floorjump,in the floor floorjump permanently disabled
        ["x"] = 5,  --when exist,hero enter stairs goto floor,enabled jump to the floor,call set_flag("floor_$(floor_id)")
        ["y"] = 10
    },
    ...
}
```

- gamemap.lua -> defines game map,the contents as follows:
```lua
gamemap =
{
    [0] =                               -- floor id
    {
        ["name"] = "第一层",            -- floor display name
        ["length"] = 2,
        ["width"] = 2,
        ["default_floorid"] = 1,        -- the floor background floor image id
        ["content"] =
        {
            {  3 ,  2 } , {  1 ,  1 }   -- there are $(length) grid
            {  2 ,  1 } , {  2 ,  1 }
            --there are $(width) grid
        }
    },
```

## Todo
- [ ] animation
- [ ] game editor
- [ ] operator undo/redo
- [ ] multiple game archives

## License
----

MIT
