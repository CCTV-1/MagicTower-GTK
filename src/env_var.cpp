#include <lua.hpp>

#include <glibmm.h>
#include <giomm.h>

#include <lua.hpp>

#include "env_var.h"
#include "resources.h"

namespace MagicTower
{
    GameEnvironment::GameEnvironment( std::vector<std::shared_ptr<const char> > music_list ):
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
        //items
        std::string item_script = CUSTOM_SCRIPTS_PATH"items.lua";
        if ( Glib::file_test( item_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing items.lua resources" );
        }
        if ( luaL_dofile( this->script_engines.get() , item_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( this->script_engines.get() , -1 ) );
        }
        lua_getglobal( this->script_engines.get() , "items" );
        luaL_checktype( this->script_engines.get() , 1 , LUA_TTABLE );
        lua_pushnil( this->script_engines.get() );
        while( lua_next( this->script_engines.get() , 1 ) )
        {
            luaL_checktype( this->script_engines.get() , 2 , LUA_TNUMBER );
            luaL_checktype( this->script_engines.get() , 3 , LUA_TTABLE );
            lua_getfield( this->script_engines.get() , 3 , "item_name" );
            lua_getfield( this->script_engines.get() , 3 , "item_detail" );
            lua_getfield( this->script_engines.get() , 3 , "item_func" );
            luaL_checktype( this->script_engines.get() , 4 , LUA_TSTRING );
            luaL_checktype( this->script_engines.get() , 5 , LUA_TSTRING );
            luaL_checktype( this->script_engines.get() , 6 , LUA_TSTRING );
            std::uint32_t item_id = lua_tointeger( this->script_engines.get() , 2 );
            std::string item_name = lua_tostring( this->script_engines.get() , 4 );
            std::string item_detail = lua_tostring( this->script_engines.get() , 5 );
            std::string item_func = lua_tostring( this->script_engines.get() , 6 );
            this->items[item_id] = { item_name , item_detail , item_func };

            lua_pop( this->script_engines.get() , 4 );
        }
        lua_pop( this->script_engines.get() , 1 );

        //stores
        std::string store_script = CUSTOM_SCRIPTS_PATH"stores.lua";
        if ( Glib::file_test( store_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing stores.lua resources" );
        }
        if ( luaL_dofile( this->script_engines.get() , store_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( this->script_engines.get() , -1 ) );
        }
        lua_getglobal( this->script_engines.get() , "stores" );
        luaL_checktype( this->script_engines.get() , 1 , LUA_TTABLE );
        lua_pushnil( this->script_engines.get() );
        while( lua_next( this->script_engines.get() , 1 ) )
        {
            luaL_checktype( this->script_engines.get() , 2 , LUA_TNUMBER );
            luaL_checktype( this->script_engines.get() , 3 , LUA_TTABLE );
            lua_getfield( this->script_engines.get() , 3 , "usability" );
            lua_getfield( this->script_engines.get() , 3 , "store_name" );
            lua_getfield( this->script_engines.get() , 3 , "commodities" );
            luaL_checktype( this->script_engines.get() , 4 , LUA_TBOOLEAN );
            luaL_checktype( this->script_engines.get() , 5 , LUA_TSTRING );
            luaL_checktype( this->script_engines.get() , 6 , LUA_TTABLE );
            std::uint32_t store_id = lua_tointeger( this->script_engines.get() , 2 );
            bool usability = lua_toboolean( this->script_engines.get() , 4 );
            std::string store_name = lua_tostring( this->script_engines.get() , 5 );

            if ( usability == true )
            {
                std::string flag_name = std::string( "store_" ) + std::to_string( store_id );
                this->script_flags[flag_name] = 1;
            }

            std::map<std::string,std::string> commodities;
            lua_pushnil( this->script_engines.get() );
            while ( lua_next( this->script_engines.get() , 6 ) )
            {
                luaL_checktype( this->script_engines.get() , 7 , LUA_TSTRING );
                luaL_checktype( this->script_engines.get() , 8 , LUA_TSTRING );
                std::string commodity_name = lua_tostring( this->script_engines.get() , 7 );
                std::string commodity_func = lua_tostring( this->script_engines.get() , 8 );
                commodities[commodity_name] = commodity_func;
                lua_pop( this->script_engines.get() , 1 );
            }
            this->store_list[ store_id ] = {
                usability , store_name , commodities
            };
            lua_pop( this->script_engines.get() , 4 );
        }
        lua_pop( this->script_engines.get() , 1 );

        //hero
        std::string hero_script = CUSTOM_SCRIPTS_PATH"hero.lua";
        if ( Glib::file_test( hero_script , Glib::FileTest::FILE_TEST_EXISTS ) == false )
        {
            throw Glib::FileError( Glib::FileError::NO_SUCH_ENTITY , "missing hero.lua resources" );
        }
        if ( luaL_dofile( this->script_engines.get() , hero_script.data() ) )
        {
            throw std::runtime_error( lua_tostring( this->script_engines.get() , -1 ) );
        }
        lua_getglobal( this->script_engines.get() , "hero_property" );
        this->hero = lua_gethero( this->script_engines.get() , 1 );

        DataBase db( DATABSE_RESOURCES_PATH );
        this->towers = db.get_tower_info( 0 );
        this->stairs = db.get_stairs_list();
        this->monsters = db.get_monster_list();
        this->access_layer = db.get_access_layers();
        this->layers_jump = db.get_jump_map();
        this->script_flags = db.get_script_flags();
        this->focus_item_id = 0;
        this->game_status = GAME_STATUS::NORMAL;
        this->path = {};
    }
}
