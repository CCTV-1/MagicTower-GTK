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
        this->items = initial_items( this->script_engines.get() );
        this->store_list = initial_stores( this->script_engines.get() , this->script_flags );
        this->hero = initial_hero( this->script_engines.get() );
        this->monsters = initial_monsters( this->script_engines.get() );

        DataBase db( DATABSE_RESOURCES_PATH );
        this->towers = db.get_tower_info( 0 );
        this->stairs = db.get_stairs_list();
        this->access_layer = db.get_access_layers();
        this->layers_jump = db.get_jump_map();
        this->script_flags = db.get_script_flags();
        this->focus_item_id = 0;
        this->game_status = GAME_STATUS::NORMAL;
        this->path = {};
    }
}
