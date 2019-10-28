# MagicTower-GTK

## Dependency Library:
  -  ```GTKMM3 3.22```+ 
  -  ```GStreamer 1.12```+
  -  ```SQLITE3 3.21```+
  -  ```jansson 2.11```+
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
--argument:table of key:string item_name , value:string item_detail_json
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
Z2FtZV9vYmplY3QK

## Todo
- [ ] animation
- [ ] game editor
- [ ] distribution script
- [ ] makefile
- [ ] test case
- [ ] debuger
- [ ] code refactoring

## License
----

MIT
