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
        focus_item_id( 0 ),
        music( music_list ),
        script_flags(),
        path( {} ),
        game_status( GAME_STATUS::NORMAL ),
        draw_path( true )
    {
        luaL_openlibs( this->script_engines.get() );
        //game environment to lua vm
        lua_pushlightuserdata( this->script_engines.get() , ( void * )this );
        //base64 game_object -> Z2FtZV9vYmplY3QK
        lua_setglobal( this->script_engines.get() , "Z2FtZV9vYmplY3QK" );

        std::string item_script = CUSTOM_SCRIPTS_PATH"items.lua";
        Glib::RefPtr<Gio::File> item_file = Gio::File::create_for_path( item_script );
        if ( item_file->query_exists() == false )
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

        DataBase db( DATABSE_RESOURCES_PATH );
        this->towers = db.get_tower_info( 0 );
        this->hero = db.get_hero_info( 0 );
        this->stairs = db.get_stairs_list();
        this->store_list = db.get_store_list();
        this->monsters = db.get_monster_list();
        this->access_layer = db.get_access_layers();
        this->layers_jump = db.get_jump_map();
        this->script_flags = db.get_script_flags();
    }

    GameEnvironment::~GameEnvironment()
    {
        ;
    }
}
