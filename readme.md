# MagicTower-GTK

## Dependency Library:
-  ```GTKMM3 3.22```+ 
-  ```GStreamer 1.12```+
-  ```SQLITE3 3.21```+
-  ```lua 5.3```+

## Language: 
- ```C++11```+

## Game Script

### Export functions

```lua
--no return
function set_tips( string )

--no return
function set_grid_type( number layer , number x , number y , number grid_id )

--return a number
function get_grid_type( number layer , number x , number y )

--hero table:
--key:layers , value:number;
--key:x , value:number;
--key:y , value:number;
--key:level , value:number;
--key:life , value:number;
--key:attack , value:number;
--key:defense , value:number;
--key:gold , value:number;
--key:experience , value:number;
--key:yellow_key , value:number;
--key:blue_key , value:number;
--key:red_key , value:number;

--return a table:
function get_hero_property( void )

--no return
function set_hero_property( table )

--if flag not exist,reutn nil.if exist,return a number
function get_flag( string flag_name )

--no return
function set_flag( string flag_name , number flag_value )

--no return
--argument:any number string(Lua c api exist limits)
function open_dialog( string dialog_content , ... )

--no return
--argument:table of key:string item_name , value:string item_func
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
function move_hero( number layer , number x , number y )

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
- store_ + any number to string
- hero_property

### script name rule

- F_$(layer)\_$(x)\_$(y).lua  --> call for each move to new position(before the built-in events)

- L_$(layer)\_$(x)\_$(y).lua  --> call for each move to new position(after the built-in event)

- items.lua --> defines game items function,the contents as follows:

```lua
items =
{
    [item_id] =                 --item_id is a number
    {
        ["item_name"] = ""      --detail dialog display text
        ["item_detail"] = "",   --detail dialog display text
        ["item_func"] = ""      --get item,call luaL_dostring(items[item_id].item_func)
    },
    ...
}
```

- hero.lua --> defines game hero initial property,the contents as follows:
```lua
hero_property=
{
    ["layers"] = 0,
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

- monsters.lua --> defines game monsters property

- stores.lua --> defines game stores function,the contents as follows:
```lua
stores=
{
    [store_id] = --sstore_id is a number.
    {
        ["usability"] = , --value is a boolean.if define true,or call unlock_store,set flag:store_id to non nul value,else to nil value.
        ["store_name"] = ,--value is a string,store menu display
        ["commodities"] =
        {
            -- commodity_detail is a string,store menu display text
            -- commodity_function is a string,when buy the commodity,pass stores[store_name].commodities.commodity_detail to call luaL_dostring
            [commodity_detail] = commodity_function,
            ...
        }
    },
    ...
}
```

## Todo
- [ ] animation
- [ ] game editor

## License
----

MIT
