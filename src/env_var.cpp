#include <glibmm.h>
#include <giomm.h>

#include <lua.hpp>

#include "env_var.h"
#include "resources.h"

namespace MagicTower
{
    static std::map<std::uint32_t,Item> initial_items( lua_State * L , std::map<std::string,std::uint32_t>& func_regmap )
    {
        if ( luaL_dofile( L , CUSTOM_SCRIPTS_PATH"items.lua" ) )
        {
            throw std::runtime_error( std::string( "do file:'items.lua' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
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
            lua_getfield( L , top + 3 , "item_detail" );
            lua_getfield( L , top + 3 , "item_func" );
            luaL_checktype( L , top + 6 , LUA_TFUNCTION );
            std::string item_name = luaL_checkstring( L , top + 4 );
            std::string item_detail = luaL_checkstring( L , top + 5 );
            items[item_id] = { item_name , item_detail };

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
            lua_pop( L , 3 );
        }
        lua_pop( L , 1 );

        return items;
    }

    static std::map<std::uint32_t,Store> initial_stores( lua_State * L , std::map<std::string , std::uint32_t>& script_flags , std::map<std::string,std::uint32_t>& func_regmap )
    {
        if ( luaL_dofile( L , CUSTOM_SCRIPTS_PATH"stores.lua" ) )
        {
            throw std::runtime_error( std::string( "do file:'stores.lua' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
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
        if ( luaL_dofile( L , CUSTOM_SCRIPTS_PATH"hero.lua" ) )
        {
            throw std::runtime_error( std::string( "do file:'hero.lua' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        lua_getglobal( L , "hero_property" );
        Hero hero = lua_gethero( L , top + 1 );
        lua_pop( L , 1 );

        return hero;
    }

    static std::map<std::uint32_t,Monster> initial_monsters( lua_State * L )
    {
        if ( luaL_dofile( L , CUSTOM_SCRIPTS_PATH"monsters.lua" ) )
        {
            throw std::runtime_error( std::string( "do file:'monsters.lua' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
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
        if ( luaL_dofile( L , CUSTOM_SCRIPTS_PATH"stairs.lua" ) )
        {
            throw std::runtime_error( std::string( "do file:'stairs.lua' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
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

    static std::map<std::size_t , std::pair<std::size_t , std::size_t>> initial_floorjump( lua_State * L )
    {
        if ( luaL_dofile( L , CUSTOM_SCRIPTS_PATH"jumper.lua" ) )
        {
            throw std::runtime_error( std::string( "do file:'jumper.lua' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        std::map<std::size_t , std::pair<std::size_t , std::size_t> > jumper;
        lua_getglobal( L , "floorjump" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            std::uint32_t jump_id = luaL_checkinteger( L , top + 2 );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "x" );
            lua_getfield( L , top + 3 , "y" );

            std::uint32_t jump_x = luaL_checkinteger( L , top + 4 );
            std::uint32_t jump_y = luaL_checkinteger( L , top + 5 );
            jumper[jump_id] = { jump_x , jump_y };
            lua_pop( L , 3 );
        }
        lua_pop( L , 1 );
        return jumper;
    }

    static TowerMap initial_gamemap( lua_State * L )
    {
        if ( luaL_dofile( L , CUSTOM_SCRIPTS_PATH"gamemap.lua" ) )
        {
            throw std::runtime_error( std::string( "do file:'gamemap.lua' failure,error message:" ) + lua_tostring( L , -1 ) );
        }
        TowerMap towers;

        std::uint32_t top = lua_gettop( L );
        std::uint32_t max_length = 0;
        std::uint32_t max_width = 0;
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
            lua_getfield( L , top + 3 , "content" );
            std::string floor_name = luaL_checkstring( L , top + 4 );
            std::uint32_t floor_length = luaL_checkinteger( L , top + 5 );
            if ( floor_length > max_length )
            {
                max_length = floor_length;
            }
            std::uint32_t floor_width = luaL_checkinteger( L , top + 6 );
            if ( floor_width > max_width )
            {
                max_width = floor_width;
            }
            std::uint32_t default_floorid = luaL_checkinteger( L , top + 7 );
            towers.map[floor_id].name = floor_name;
            towers.map[floor_id].length = floor_length;
            towers.map[floor_id].width = floor_width;
            towers.map[floor_id].default_floorid = default_floorid;

            luaL_checktype( L , top + 8 , LUA_TTABLE );
            lua_pushnil( L );
            while( lua_next( L , top + 8 ) )  //floor content table
            {
                luaL_checktype( L , top + 9 , LUA_TNUMBER );
                luaL_checktype( L , top + 10 , LUA_TTABLE );   //grid table
                lua_rawgeti( L , top + 10 , 1 );
                lua_rawgeti( L , top + 10 , 2 );
                std::uint32_t grid_type = luaL_checkinteger( L , top + 11 );
                std::uint32_t grid_id = luaL_checkinteger( L , top + 12 );
                towers.map[floor_id].content.push_back({ static_cast<GRID_TYPE>( grid_type ) , grid_id });
                lua_pop( L , 3 );
            }

            lua_pop( L , 6 );
        }
        lua_pop( L , 1 );
        towers.MAX_LENGTH = max_length;
        towers.MAX_WIDTH = max_width;

        return towers;
    }

    GameEnvironment::GameEnvironment( std::vector<std::string> music_list ):
        draw_path( true ),
        focus_item_id(),
        game_message( {} ),
        tips_content( {} ),
        script_flags(),
        script_engines( luaL_newstate() , lua_close ),
        path( {} ),
        menu_items( {} ),
        music( music_list ),
        game_status()
    {
        luaL_openlibs( this->script_engines.get() );
        //game environment to lua vm
        lua_pushlightuserdata( this->script_engines.get() , ( void * )this );
        //base64 game_object -> Z2FtZV9vYmplY3QK
        lua_setglobal( this->script_engines.get() , "Z2FtZV9vYmplY3QK" );

        this->initial_gamedata();
    }

    GameEnvironment::~GameEnvironment()
    {
        for ( auto& func : this->refmap )
        {
            luaL_unref( this->script_engines.get() , LUA_REGISTRYINDEX , func.second );
        }
    }

    void GameEnvironment::initial_gamedata()
    {
        this->items = initial_items( this->script_engines.get() , this->refmap );
        this->stores = initial_stores( this->script_engines.get() , this->script_flags , this->refmap );
        this->hero = initial_hero( this->script_engines.get() );
        this->monsters = initial_monsters( this->script_engines.get() );
        this->stairs = initial_stairs( this->script_engines.get() );
        this->floors_jump = initial_floorjump( this->script_engines.get() );
        this->game_map = initial_gamemap( this->script_engines.get() );

        this->focus_item_id = 0;
        this->game_status = GAME_STATUS::NORMAL;
        this->path = {};
    }
}
