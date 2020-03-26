#include <glibmm.h>
#include <giomm.h>

#include <lua.hpp>

#include "env_var.h"
#include "resources.h"

namespace MagicTower
{
    static int getter( lua_State * L )
    {
        luaL_error( L , "script attempted to access nonexistent global variable" );
        return 0;
    }

    static int setter( lua_State * L )
    {
        luaL_error( L , "script attempted to create global variable" );
        return 0;
    }

    static void do_textscript( lua_State * L , std::string filename , std::uint32_t arg_count = 0 , std::uint32_t ret_count = 0 )
    {
        int res = luaL_loadfilex( L , filename.c_str() , "t" );
        if ( res != LUA_OK )
        {
            throw std::runtime_error( std::string( "load file:'" ) + filename + std::string( "' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
        if ( lua_pcall( L , arg_count , ret_count , 0 ) != LUA_OK )
        {
            throw std::runtime_error( std::string( "execute file:'" ) + filename + std::string( "' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
    }

    static void initial_sandbox( lua_State * L )
    {
        const char * unsafe_funcnames[] =
        {
            "load",
            "loadfile",
            "loadstring",
            "dofile",
            "collectgarbage",
            "getmetatable",
            "rawget",
            "rawset",
            "rawequal",
            "require",
            "package",
            "debug",
            "os",
            "io"
        };
        for ( std::size_t i = 0 ; i < sizeof( unsafe_funcnames )/sizeof( const char * ) ; i++ )
        {
            lua_pushnil( L );
            lua_setglobal( L , unsafe_funcnames[i] );
        }

        const char * global_variablenames[] =
        {
            "items",
            "stores",
            "hero_property",
            "monsters",
            "stairs",
            "gamemap"
        };
        for ( std::size_t i = 0 ; i < sizeof( global_variablenames )/sizeof( const char * ) ; i++ )
        {
            lua_pushboolean( L , true );
            lua_setglobal( L , global_variablenames[i] );
        }
        //disallow:create global variable,access non-exist global variable
        int top = lua_gettop( L );
        lua_getglobal( L , "_G" );
        lua_newtable( L );
        lua_pushstring( L , "__index" );
        lua_pushcfunction( L , &getter );
        lua_rawset( L , top + 2 );
        lua_pushstring( L , "__newindex" );
        lua_pushcfunction( L , &setter );
        lua_rawset( L , top + 2 );
        lua_pushstring( L , "__metatable" );
        lua_newtable( L );
        lua_rawset( L , top + 2 );
        lua_setmetatable( L , top + 1 );
        lua_pop( L , 1 );
    }

    static std::map<std::uint32_t,Item> initial_items( lua_State * L , std::map<std::string,std::uint32_t>& func_regmap )
    {
        do_textscript( L , ResourcesManager::get_script_path() + "items.lua" );
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Item> items;
        lua_getglobal( L , "items" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            std::uint32_t item_id = luaL_checkinteger( L , top + 2 );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "item_name" );
            lua_getfield( L , top + 3 , "item_type" );
            lua_getfield( L , top + 3 , "item_detail" );
            lua_getfield( L , top + 3 , "item_func" );
            luaL_checktype( L , top + 7 , LUA_TFUNCTION );
            std::string item_name = luaL_checkstring( L , top + 4 );
            bool needactive = true;
            if ( lua_isnil( L , top + 5 ) )
            {
                needactive = false;
            }
            std::string item_detail = luaL_checkstring( L , top + 6 );
            items[item_id] = { needactive , item_name , item_detail };

            if ( func_regmap.find( item_detail ) == func_regmap.end() )
            {
                //top + 3 -> stack top
                std::uint32_t refkey = luaL_ref( L , LUA_REGISTRYINDEX );
                func_regmap[item_detail] = refkey;
            }
            else
            {
                //luaL_ref pop top,
                lua_pop( L , 1 );
            }
            lua_pop( L , 4 );
        }
        lua_pop( L , 1 );

        return items;
    }

    static std::map<std::uint32_t,Store> initial_stores( lua_State * L , std::map<std::string , std::uint32_t>& script_flags , std::map<std::string,std::uint32_t>& func_regmap )
    {
        do_textscript( L , ResourcesManager::get_script_path() + "stores.lua" );
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Store> stores;
        lua_getglobal( L , "stores" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            std::uint32_t store_id = luaL_checkinteger( L , top + 2 );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "usability" );
            lua_getfield( L , top + 3 , "store_name" );
            lua_getfield( L , top + 3 , "commodities" );
            luaL_checktype( L , top + 4 , LUA_TBOOLEAN );
            bool usability = lua_toboolean( L , top + 4 );
            std::string store_name = luaL_checkstring( L , top + 5 );
            luaL_checktype( L , top + 6 , LUA_TTABLE );

            if ( usability == true )
            {
                std::string flag_name = std::string( "store_" ) + std::to_string( store_id );
                script_flags[flag_name] = 1;
            }

            std::vector<std::string> commodities;
            lua_pushnil( L );
            while ( lua_next( L , top + 6 ) )
            {
                std::string commodity_name = luaL_checkstring( L , top + 7 );
                luaL_checktype( L , top + 8 , LUA_TFUNCTION );
                if ( func_regmap.find( commodity_name ) == func_regmap.end() )
                {
                    commodities.push_back( commodity_name );
                    //top + 3 -> stack top
                    std::uint32_t refkey = luaL_ref( L , LUA_REGISTRYINDEX );
                    func_regmap[commodity_name] = refkey;
                }
                else
                {
                    //luaL_ref pop top + 3,if call don't need pop
                    lua_pop( L , 1 );
                }
            }
            stores[ store_id ] = {
                usability , store_name , commodities
            };
            lua_pop( L , 4 );
        }
        lua_pop( L , 1 );

        return stores;
    }

    static Hero initial_hero( lua_State * L )
    {
        do_textscript( L , ResourcesManager::get_script_path() + "hero.lua" );
        std::uint32_t top = lua_gettop( L );
        lua_getglobal( L , "hero_property" );
        Hero hero = lua_gethero( L , top + 1 );
        lua_pop( L , 1 );

        return hero;
    }

    static std::map<std::uint32_t,Monster> initial_monsters( lua_State * L )
    {
        do_textscript( L , ResourcesManager::get_script_path() + "monsters.lua" );
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Monster> monsters;
        lua_getglobal( L , "monsters" );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            std::uint32_t monster_id = luaL_checkinteger( L , top + 2 );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "attack_type" );
            lua_getfield( L , top + 3 , "type_value" );
            lua_getfield( L , top + 3 , "name" );
            lua_getfield( L , top + 3 , "level" );
            lua_getfield( L , top + 3 , "life" );
            lua_getfield( L , top + 3 , "attack" );
            lua_getfield( L , top + 3 , "defense" );
            lua_getfield( L , top + 3 , "gold" );
            lua_getfield( L , top + 3 , "experience" );

            Monster monster;
            monster.type = static_cast<ATTACK_TYPE>( luaL_checkinteger( L , top + 4 ) );
            monster.type_value = luaL_checkinteger( L , top + 5 );
            monster.name = luaL_checkstring( L , top + 6 );
            monster.level = luaL_checkinteger( L , top + 7 );
            monster.life = luaL_checkinteger( L , top + 8 );
            monster.attack = luaL_checkinteger( L , top + 9 );
            monster.defense = luaL_checkinteger( L , top + 10 );
            monster.gold = luaL_checkinteger( L , top + 11 );
            monster.experience = luaL_checkinteger( L , top + 12 );
            monsters[monster_id] = monster;
            lua_pop( L , 10 );
        }
        lua_pop( L , 1 );

        return monsters;
    }

    static std::map<std::uint32_t,Stairs> initial_stairs( lua_State * L )
    {
        do_textscript( L , ResourcesManager::get_script_path() + "stairs.lua" );
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Stairs> stairs;
        lua_getglobal( L , "stairs" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            std::uint32_t stairs_id = luaL_checkinteger( L , top + 2 );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "image_type" );
            lua_getfield( L , top + 3 , "floor" );
            lua_getfield( L , top + 3 , "x" );
            lua_getfield( L , top + 3 , "y" );
            std::uint32_t stairs_type = luaL_checkinteger( L , top + 4 );
            std::uint32_t stairs_floor = luaL_checkinteger( L , top + 5 );
            std::uint32_t stairs_x = luaL_checkinteger( L , top + 6 );
            std::uint32_t stairs_y = luaL_checkinteger( L , top + 7 );
            stairs[stairs_id] = { stairs_type , stairs_floor , stairs_x , stairs_y };
            lua_pop( L , 5 );
        }
        lua_pop( L , 1 );
        return stairs;
    }

    static TowerMap initial_gamemap( lua_State * L )
    {
        do_textscript( L , ResourcesManager::get_script_path() + "gamemap.lua" );
        TowerMap towers;

        std::uint32_t top = lua_gettop( L );
        lua_getglobal( L , "gamemap" );
        luaL_checktype( L , top + 1 , LUA_TTABLE ); //map table
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )  //floor info table
        {
            std::uint32_t floor_id = luaL_checkinteger( L , top + 2 );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "name" );
            lua_getfield( L , top + 3 , "length" );
            lua_getfield( L , top + 3 , "width" );
            lua_getfield( L , top + 3 , "default_floorid" );
            lua_getfield( L , top + 3 , "teleport" );
            lua_getfield( L , top + 3 , "field_vision" );
            lua_getfield( L , top + 3 , "content" );
            std::string floor_name = luaL_checkstring( L , top + 4 );
            std::uint32_t floor_length = luaL_checkinteger( L , top + 5 );
            std::uint32_t floor_width = luaL_checkinteger( L , top + 6 );
            std::uint32_t default_floorid = luaL_checkinteger( L , top + 7 );
            if ( lua_istable( L , top + 8 ) )
            {
                lua_getfield( L , top + 8 , "x" );
                lua_getfield( L , top + 8 , "y" );
                //top + 10 has content table
                std::uint32_t tp_x = luaL_checkinteger( L , top + 11 );
                std::uint32_t tp_y = luaL_checkinteger( L , top + 12 );
                lua_pop( L , 2 );
                towers.map[floor_id].teleport_point = { tp_x , tp_y };
            }
            else
            {
                towers.map[floor_id].teleport_point = std::nullopt;
            }

            if ( lua_istable( L , top + 9 ) )
            {
                lua_getfield( L , top + 9 , "x" );
                lua_getfield( L , top + 9 , "y" );
                //top + 10 has content table
                std::uint32_t tp_x = luaL_checkinteger( L , top + 11 );
                std::uint32_t tp_y = luaL_checkinteger( L , top + 12 );
                lua_pop( L , 2 );
                towers.map[floor_id].field_vision = { tp_x , tp_y };
            }
            else
            {
                towers.map[floor_id].field_vision = std::nullopt;
            }

            towers.map[floor_id].name = floor_name;
            towers.map[floor_id].length = floor_length;
            towers.map[floor_id].width = floor_width;
            towers.map[floor_id].default_floorid = default_floorid;

            luaL_checktype( L , top + 10 , LUA_TTABLE );
            lua_pushnil( L );
            while( lua_next( L , top + 10 ) )  //floor content table
            {
                luaL_checktype( L , top + 11 , LUA_TNUMBER );
                luaL_checktype( L , top + 12 , LUA_TTABLE );   //grid table
                lua_rawgeti( L , top + 12 , 1 );
                lua_rawgeti( L , top + 12 , 2 );
                std::uint32_t grid_type = luaL_checkinteger( L , top + 13 );
                std::uint32_t grid_id = luaL_checkinteger( L , top + 14 );
                towers.map[floor_id].content.push_back({ static_cast<GRID_TYPE>( grid_type ) , grid_id });
                lua_pop( L , 3 );
            }

            lua_pop( L , 8 );
        }
        lua_pop( L , 1 );

        return towers;
    }

    GameEnvironment::GameEnvironment( std::vector<std::string> music_list ):
        draw_path( true ),
        focus_item_id(),
        game_message( {} ),
        tips_content( {} ),
        inventories({}),
        script_flags(),
        script_engines( luaL_newstate() , lua_close ),
        path( {} ),
        menu_items( {} ),
        music( music_list ),
        soundeffect_player( std::vector<std::string>({}) ),
        game_status()
    {
        lua_State * L = this->script_engines.get();
        luaL_openlibs( L );
        initial_sandbox( L );
        this->initial_gamedata();

        soundeffect_player.set_play_mode(PLAY_MODE::SINGLE_PLAY);
    }

    GameEnvironment::~GameEnvironment()
    {
        lua_State * L = this->script_engines.get();
        for ( auto& func : this->refmap )
        {
            luaL_unref( L , LUA_REGISTRYINDEX , func.second );
        }
    }

    void GameEnvironment::initial_gamedata()
    {
        lua_State * L = this->script_engines.get();
        this->items = initial_items( L , this->refmap );
        this->stores = initial_stores( L , this->script_flags , this->refmap );
        this->hero = initial_hero( L );
        this->monsters = initial_monsters( L );
        this->stairs = initial_stairs( L );
        this->game_map = initial_gamemap( L );

        this->focus_item_id = 0;
        this->game_status = GAME_STATUS::NORMAL;
        this->path = {};
    }
}
