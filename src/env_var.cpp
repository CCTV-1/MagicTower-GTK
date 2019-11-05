#include <lua.hpp>

#include <glibmm.h>
#include <giomm.h>

#include <lua.hpp>

#include "env_var.h"
#include "resources.h"

namespace MagicTower
{
    static std::map<std::uint32_t,Item> initial_items( lua_State * L )
    {
        std::string item_script = CUSTOM_SCRIPTS_PATH"items.lua";
        if ( Glib::file_test( item_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing items.lua resources" );
        }
        if ( luaL_dofile( L , item_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Item> items;
        lua_getglobal( L , "items" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            luaL_checktype( L , top + 2 , LUA_TNUMBER );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "item_name" );
            lua_getfield( L , top + 3 , "item_detail" );
            lua_getfield( L , top + 3 , "item_func" );
            luaL_checktype( L , top + 4 , LUA_TSTRING );
            luaL_checktype( L , top + 5 , LUA_TSTRING );
            luaL_checktype( L , top + 6 , LUA_TSTRING );
            std::uint32_t item_id = lua_tointeger( L , top + 2 );
            std::string item_name = lua_tostring( L , top + 4 );
            std::string item_detail = lua_tostring( L , top + 5 );
            std::string item_func = lua_tostring( L , top + 6 );
            items[item_id] = { item_name , item_detail , item_func };

            lua_pop( L , 4 );
        }
        lua_pop( L , 1 );

        return items;
    }

    static std::map<std::uint32_t,Store> initial_stores( lua_State * L , std::map<std::string , std::uint32_t>& script_flags )
    {
        std::string store_script = CUSTOM_SCRIPTS_PATH"stores.lua";
        if ( Glib::file_test( store_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing stores.lua resources" );
        }
        if ( luaL_dofile( L , store_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Store> stores;
        lua_getglobal( L , "stores" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            luaL_checktype( L , top + 2 , LUA_TNUMBER );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "usability" );
            lua_getfield( L , top + 3 , "store_name" );
            lua_getfield( L , top + 3 , "commodities" );
            luaL_checktype( L , top + 4 , LUA_TBOOLEAN );
            luaL_checktype( L , top + 5 , LUA_TSTRING );
            luaL_checktype( L , top + 6 , LUA_TTABLE );
            std::uint32_t store_id = lua_tointeger( L , top + 2 );
            bool usability = lua_toboolean( L , top + 4 );
            std::string store_name = lua_tostring( L , top + 5 );

            if ( usability == true )
            {
                std::string flag_name = std::string( "store_" ) + std::to_string( store_id );
                script_flags[flag_name] = 1;
            }

            std::map<std::string,std::string> commodities;
            lua_pushnil( L );
            while ( lua_next( L , top + 6 ) )
            {
                luaL_checktype( L , top + 7 , LUA_TSTRING );
                luaL_checktype( L , top + 8 , LUA_TSTRING );
                std::string commodity_name = lua_tostring( L , top + 7 );
                std::string commodity_func = lua_tostring( L , top + 8 );
                commodities[commodity_name] = commodity_func;
                lua_pop( L , 1 );
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
        std::string hero_script = CUSTOM_SCRIPTS_PATH"hero.lua";
        if ( Glib::file_test( hero_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing hero.lua resources" );
        }
        if ( luaL_dofile( L , hero_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        lua_getglobal( L , "hero_property" );
        Hero hero = lua_gethero( L , top + 1 );
        lua_pop( L , 1 );

        return hero;
    }

    static std::map<std::uint32_t,Monster> initial_monsters( lua_State * L )
    {
        std::string monsters_script = CUSTOM_SCRIPTS_PATH"monsters.lua";
        if ( Glib::file_test( monsters_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing monsters.lua resources" );
        }
        if ( luaL_dofile( L , monsters_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Monster> monsters;
        lua_getglobal( L , "monsters" );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            luaL_checktype( L , top + 2 , LUA_TNUMBER );
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
            luaL_checktype( L , top + 4  , LUA_TNUMBER );
            luaL_checktype( L , top + 5  , LUA_TNUMBER );
            luaL_checktype( L , top + 6  , LUA_TSTRING );
            luaL_checktype( L , top + 7  , LUA_TNUMBER );
            luaL_checktype( L , top + 8  , LUA_TNUMBER );
            luaL_checktype( L , top + 9  , LUA_TNUMBER );
            luaL_checktype( L , top + 10 , LUA_TNUMBER );
            luaL_checktype( L , top + 11 , LUA_TNUMBER );
            luaL_checktype( L , top + 12 , LUA_TNUMBER );
            
            Monster monster;
            std::uint32_t monster_id = lua_tointeger( L , top + 2 );
            monster.type = static_cast<ATTACK_TYPE>( lua_tointeger( L , top + 4 ) );
            monster.type_value = lua_tointeger( L , top + 5 );
            monster.name = lua_tostring( L , top + 6 );
            monster.level = lua_tointeger( L , top + 7 );
            monster.life = lua_tointeger( L , top + 8 );
            monster.attack = lua_tointeger( L , top + 9 );
            monster.defense = lua_tointeger( L , top + 10 );
            monster.gold = lua_tointeger( L , top + 11 );
            monster.experience = lua_tointeger( L , top + 12 );
            monsters[monster_id] = monster;
            lua_pop( L , 10 );
        }
        lua_pop( L , 1 );

        return monsters;
    }

    static std::map<std::uint32_t,Stairs> initial_stairs( lua_State * L )
    {
        std::string stairs_script = CUSTOM_SCRIPTS_PATH"stairs.lua";
        if ( Glib::file_test( stairs_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing monsters.lua resources" );
        }
        if ( luaL_dofile( L , stairs_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        std::map<std::uint32_t,Stairs> stairs;
        lua_getglobal( L , "stairs" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            luaL_checktype( L , top + 2 , LUA_TNUMBER );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "image_type" );
            lua_getfield( L , top + 3 , "floor" );
            lua_getfield( L , top + 3 , "x" );
            lua_getfield( L , top + 3 , "y" );
            luaL_checktype( L , top + 4 , LUA_TNUMBER );
            luaL_checktype( L , top + 5 , LUA_TNUMBER );
            luaL_checktype( L , top + 6 , LUA_TNUMBER );
            luaL_checktype( L , top + 7 , LUA_TNUMBER );
            std::uint32_t stairs_id = lua_tointeger( L , top + 2 );
            std::uint32_t stairs_type = lua_tointeger( L , top + 4 );
            std::uint32_t stairs_floor = lua_tointeger( L , top + 5 );
            std::uint32_t stairs_x = lua_tointeger( L , top + 6 );
            std::uint32_t stairs_y = lua_tointeger( L , top + 7 );
            stairs[stairs_id] = { stairs_type , stairs_floor , stairs_x , stairs_y };
            lua_pop( L , 5 );
        }
        lua_pop( L , 1 );
        return stairs;
    }

    static std::map<std::size_t , std::pair<std::size_t , std::size_t> > initial_floorjump( lua_State * L )
    {
        std::string jumper_script = CUSTOM_SCRIPTS_PATH"jumper.lua";
        if ( Glib::file_test( jumper_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing jumper.lua resources" );
        }
        if ( luaL_dofile( L , jumper_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( L , -1 ) );
        }
        std::uint32_t top = lua_gettop( L );
        std::map<std::size_t , std::pair<std::size_t , std::size_t> > jumper;
        lua_getglobal( L , "floorjump" );
        luaL_checktype( L , top + 1 , LUA_TTABLE );
        lua_pushnil( L );
        while( lua_next( L , top + 1 ) )
        {
            luaL_checktype( L , top + 2 , LUA_TNUMBER );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "x" );
            lua_getfield( L , top + 3 , "y" );
            luaL_checktype( L , top + 4 , LUA_TNUMBER );
            luaL_checktype( L , top + 5 , LUA_TNUMBER );

            std::uint32_t jump_id = lua_tointeger( L , top + 2 );
            std::uint32_t jump_x = lua_tointeger( L , top + 4 );
            std::uint32_t jump_y = lua_tointeger( L , top + 5 );
            jumper[jump_id] = { jump_x , jump_y };
            lua_pop( L , 3 );
        }
        lua_pop( L , 1 );
        return jumper;
    }

    static TowerMap initial_gamemap( lua_State * L )
    {
        std::string gamemap_script = CUSTOM_SCRIPTS_PATH"gamemap.lua";
        if ( Glib::file_test( gamemap_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing gamemap.lua resources" );
        }
        if ( luaL_dofile( L , gamemap_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( L , -1 ) );
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
            luaL_checktype( L , top + 2 , LUA_TNUMBER );
            luaL_checktype( L , top + 3 , LUA_TTABLE );
            lua_getfield( L , top + 3 , "name" );
            lua_getfield( L , top + 3 , "length" );
            lua_getfield( L , top + 3 , "width" );
            lua_getfield( L , top + 3 , "default_floorid" );
            lua_getfield( L , top + 3 , "content" );
            luaL_checktype( L , top + 4 , LUA_TSTRING );
            luaL_checktype( L , top + 5 , LUA_TNUMBER );
            luaL_checktype( L , top + 6 , LUA_TNUMBER );
            luaL_checktype( L , top + 7 , LUA_TNUMBER );
            luaL_checktype( L , top + 8 , LUA_TTABLE );
            std::uint32_t floor_id = lua_tointeger( L , top + 2 );
            std::string floor_name = lua_tostring( L , top + 4 );
            std::uint32_t floor_length = lua_tointeger( L , top + 5 );
            if ( floor_length > max_length )
            {
                max_length = floor_length;
            }
            std::uint32_t floor_width = lua_tointeger( L , top + 6 );
            if ( floor_width > max_width )
            {
                max_width = floor_width;
            }
            std::uint32_t default_floorid = lua_tointeger( L , top + 7 );
            towers.map[floor_id].name = floor_name;
            towers.map[floor_id].length = floor_length;
            towers.map[floor_id].width = floor_width;
            towers.map[floor_id].default_floorid = default_floorid;

            lua_pushnil( L );
            while( lua_next( L , top + 8 ) )  //floor content table
            {
                luaL_checktype( L , top + 9 , LUA_TNUMBER );
                luaL_checktype( L , top + 10 , LUA_TTABLE );   //grid table
                lua_rawgeti( L , top + 10 , 1 );
                lua_rawgeti( L , top + 10 , 2 );
                luaL_checktype( L , top + 11 , LUA_TNUMBER );
                luaL_checktype( L , top + 12 , LUA_TNUMBER );
                std::uint32_t grid_type = lua_tointeger( L , top + 11 );
                std::uint32_t grid_id = lua_tointeger( L , top + 12 );
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
        script_engines( luaL_newstate() , lua_close ),
        game_message( {} ),
        tips_content( {} ),
        menu_items( {} ),
        focus_item_id(),
        music( music_list ),
        script_flags(),
        path( {} ),
        game_status(),
        draw_path( true )
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
        ;
    }

    void GameEnvironment::initial_gamedata()
    {
        this->items = initial_items( this->script_engines.get() );
        this->stores = initial_stores( this->script_engines.get() , this->script_flags );
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
