#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cinttypes>

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <array>
#include <vector>
#include <utility>
#include <queue>
#include <tuple>
#include <algorithm>

#include <glibmm.h> //g_log
#include <giomm.h>

#include <lua.hpp>

#include "game_event.h"
#include "env_var.h"
#include "resources.h"

namespace MagicTower
{
    //floor , x , y
    std::tuple<std::uint32_t,std::uint32_t,std::uint32_t> temp_pos;

    static std::int64_t get_combat_damage_of_last  ( const Hero& hero , const Monster& monster );
    static std::int64_t get_combat_damage_of_normal( const Hero& hero , const Monster& monster );
    static std::int64_t get_combat_damage_of_first ( const Hero& hero , const Monster& monster );
    static std::int64_t get_combat_damage_of_double( const Hero& hero , const Monster& monster );
    static bool try_jump( GameEnvironment * game_object );
    static void set_jump_menu( GameEnvironment * game_object );
    static void set_start_menu( GameEnvironment * game_object );
    static void set_game_menu( GameEnvironment * game_object );
    static void set_store_menu( GameEnvironment * game_object );
    static void set_sub_store_menu( GameEnvironment * game_object , std::uint32_t store_id );
    static void remove_tips( GameEnvironment * game_object );

    // Helpers for TowerGridLocation
    static bool operator==( TowerGridLocation a , TowerGridLocation b )
    {
        return a.x == b.x && a.y == b.y;
    }

    static bool operator!=( TowerGridLocation a , TowerGridLocation b )
    {
        return !( a == b );
    }

    static bool operator<( TowerGridLocation a, TowerGridLocation b )
    {
        return std::tie( a.x , a.y ) < std::tie( b.x , b.y );
    }

    struct SquareGrid
    {
        std::array<TowerGridLocation, 4> DIRS =
        {
            TowerGridLocation{ 1 , 0  } , TowerGridLocation{ 0 , -1 } ,
            TowerGridLocation{ -1 , 0 } , TowerGridLocation{ 0 , 1 }
        };

        std::int64_t width, height;
        std::set<TowerGridLocation> walls;

        SquareGrid(std::int64_t width_, std::int64_t height_)
            : width( width_ ), height( height_ ) {}

        bool in_bounds( TowerGridLocation id ) const
        {
            return 0 <= id.x && id.x < width && 0 <= id.y && id.y < height;
        }

        bool passable( TowerGridLocation id ) const
        {
            return walls.find( id ) == walls.end();
        }

        std::vector<TowerGridLocation> neighbors( TowerGridLocation id ) const
        {
            std::vector<TowerGridLocation> results;

            for ( TowerGridLocation dir : DIRS )
            {
                TowerGridLocation next{ id.x + dir.x , id.y + dir.y };
                if ( in_bounds( next ) && passable( next ) )
                {
                    results.push_back( next );
                }
            }

            if ( ( id.x + id.y ) % 2 == 0 )
            {
                // aesthetic improvement on square grids
                std::reverse( results.begin() , results.end() );
            }

            return results;
        }
    };

    template <typename T, typename priority_t>
    struct PriorityQueue
    {
        typedef std::pair<priority_t, T> PQElement;
        std::priority_queue<PQElement, std::vector<PQElement>,
                            std::greater<PQElement> > elements;

        inline bool empty() const
        {
            return elements.empty();
        }

        inline void put( T item , priority_t priority )
        {
            elements.emplace( priority , item );
        }

        T get()
        {
            T best_item = elements.top().second;
            elements.pop();
            return best_item;
        }
    };


    struct GridWithWeights : SquareGrid
    {
        std::set<TowerGridLocation> forests;
        GridWithWeights( std::int64_t w, std::int64_t h ) : SquareGrid( w , h ) {}
        double cost( TowerGridLocation , TowerGridLocation to_node ) const
        {
            //efficiency:1;bypass any forests:this->width*this->height;so 
            return forests.find( to_node ) != forests.end() ? this->width + this->height : 1;
        }
    };

    template <typename Location>
    static std::vector<Location> reconstruct_path( Location start , Location goal ,
        std::map<Location , Location> came_from )
    {
        std::vector<Location> path;
        Location current = goal;
        while ( current != start )
        {
            path.push_back( current );
            current = came_from[current];
            //path not found return
            if ( came_from.find( current ) == came_from.end() )
                return {};
        }
        path.push_back( start ); // optional
        //remove self location
        path.pop_back();
        //std::reverse( path.begin() , path.end() );
        return path;
    }

    static inline double heuristic( TowerGridLocation a , TowerGridLocation b )
    {
        return std::abs( a.x -  b.x ) + std::abs( a.y -  b.y );
    }

    template <typename Location, typename Graph>
    static void a_star_search( Graph graph , Location start , Location goal,
        std::map<Location, Location> &came_from , std::map<Location, double> &cost_so_far )
    {
        PriorityQueue<Location, double> frontier;
        frontier.put( start , 0 );

        came_from[start] = start;
        cost_so_far[start] = 0;

        while ( !frontier.empty() )
        {
            Location current = frontier.get();

            if ( current == goal )
            {
                break;
            }

            for ( Location next : graph.neighbors( current ) )
            {
                double new_cost = cost_so_far[current] + graph.cost( current , next );
                if ( cost_so_far.find( next ) == cost_so_far.end() || new_cost < cost_so_far[next] )
                {
                    cost_so_far[next] = new_cost;
                    double priority = new_cost + heuristic( next , goal );
                    frontier.put( next , priority );
                    came_from[next] = current;
                }
            }
        }
    }

    void scriptengines_register_eventfunc( GameEnvironment * game_object )
    {
        lua_State * script_engines = game_object->script_engines.get();
        //void set_tips( string )
        lua_register( script_engines , "set_tips" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_set_tips" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 1 );
                const char * tips = luaL_checkstring( L , 1 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                set_tips( game_object , tips );
                return 0;
            }
        );
        //void set_grid_type( number floor , number x , number y , number grid_id )
        lua_register( script_engines , "set_grid_type" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 4 )
                {
                    g_log( "lua_set_grid_type" , G_LOG_LEVEL_WARNING , "expecting exactly 4 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 4 );
                std::uint32_t floor = luaL_checkinteger( L , 1 );
                std::uint32_t x = luaL_checkinteger( L , 2 );
                std::uint32_t y = luaL_checkinteger( L , 3 );
                std::uint32_t grid_id = luaL_checkinteger( L , 4 );
                GRID_TYPE grid_type = static_cast<GRID_TYPE>( grid_id );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 5 );
                
                set_grid_type( game_object , { x , y , floor } , grid_type );
                return 0;
            }
        );
        //number get_grid_type( number floor , number x , number y )
        lua_register( script_engines , "get_grid_type" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 3 )
                {
                    g_log( "lua_get_grid_type" , G_LOG_LEVEL_WARNING , "expecting exactly 3 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 3 );
                std::uint32_t floor = luaL_checkinteger( L , 1 );
                std::uint32_t x = luaL_checkinteger( L , 2 );
                std::uint32_t y = luaL_checkinteger( L , 3 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 4 );
                TowerGrid grid = game_object->game_map.get_grid( floor , x , y );
                lua_pushnumber( L , static_cast<std::uint32_t>( grid.type ) );
                return 1;
            }
        );
        //table get_hero_property( void )
        lua_register( script_engines , "get_hero_property" ,
            []( lua_State * L ) -> int
            {
                //arguments number impossible less than 0,don't need check
                //discard any extra arguments passed
                lua_settop( L , 0 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );
                Hero& hero = game_object->hero;
                lua_pushhero( L , hero );
                return 1;
            }
        );
        //void set_hero_property( table )
        lua_register( script_engines , "set_hero_property" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_set_hero_property" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 1 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                game_object->hero = lua_gethero( L , 1 );
                return 0;
            }
        );
        //number get_flag( string flag_name )
        //if flag not exist,reutn nil
        lua_register( script_engines , "get_flag" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_get_flag" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 1 );
                std::string flags_name( luaL_checkstring( L , 1 ) );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                if ( game_object->script_flags.find( flags_name ) == game_object->script_flags.end() )
                {
                    lua_pushnil( L );
                    return 1;
                }
                std::int64_t value = game_object->script_flags[flags_name];
                lua_pushnumber( L , value );
                return 1;
            }
        );
        //void set_flag( string flag_name , number flag_value )
        lua_register( script_engines , "set_flag" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 2 )
                {
                    g_log( "lua_set_flag" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 2 );
                std::string flags_name( luaL_checkstring( L , 1 ) );
                luaL_checktype( L , 2 , LUA_TNUMBER );
                std::int64_t value = lua_tointeger( L , 2 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 3 );
                game_object->script_flags[flags_name] = value;
                return 0;
            }
        );
        //void open_dialog( string list dialog_content )
        lua_register( script_engines , "open_dialog" ,
            []( lua_State * L ) -> int
            {
                std::size_t argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_get_item" , G_LOG_LEVEL_WARNING , "expecting exactly >= 1 arguments" );
                    return 0;
                }
                std::deque<std::string> messages;
                for ( std::size_t i = 1 ; i <= argument_count ; i++ )
                {
                    messages.push_back( luaL_checkstring( L , i ) );
                }
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , argument_count + 1 );
                game_object->game_message = messages;
                game_object->game_status = GAME_STATUS::MESSAGE;
                return 0;
            }
        );
        //void open_menu( table of ( string item_name , string item_func ) )
        lua_register( script_engines , "open_menu" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_open_menu" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 1 );
                luaL_checktype( L , 1 , LUA_TTABLE );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                game_object->menu_items = {};
                lua_pop( L , 1 );
                lua_pushnil( L );
                while( lua_next( L , 1 ) )
                {
                    if ( ( lua_type( L , -1 ) == LUA_TSTRING ) && ( lua_type( L , -2 ) == LUA_TSTRING ) )
                    {
                        std::string item_name( lua_tostring( L , -2 ) );
                        std::string item_func( lua_tostring( L , -1 ) );
                        game_object->menu_items.push_back({
                            [ item_name ](){ return item_name; },
                            [ L , item_func ](){ luaL_dostring( L , item_func.c_str() ); }
                        });
                    }
                    lua_pop( L , 1 );
                }
                game_object->menu_items.push_back({
                    [](){ return std::string( "关闭菜单" ); },
                    [ game_object ](){ game_object->game_status = GAME_STATUS::NORMAL; }
                });
                game_object->game_status = GAME_STATUS::GAME_MENU;
                game_object->focus_item_id = 0;
                return 0;
            }
        );
        //void close_menu( void )
        lua_register( script_engines , "close_menu" ,
            []( lua_State * L ) -> int
            {
                lua_settop( L , 0 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );
                if ( game_object->game_status == GAME_STATUS::GAME_MENU )
                {
                    game_object->game_status = GAME_STATUS::NORMAL;
                }
                return 0;
            }
        );
        //void get_item( number item_id )
        lua_register( script_engines , "get_item" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_get_item" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 1 );
                std::uint32_t item_id = luaL_checkinteger( L , 1 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                
                get_item( game_object , item_id );
                return 0;
            }
        );
        //void unlock_store( number store_id )
        lua_register( script_engines , "unlock_store" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_unlock_store" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 1 );
                std::uint32_t store_id = luaL_checkinteger( L , 1 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                game_object->stores[ store_id ].usability = true;
                std::string flag_name = std::string( "store_" ) + std::to_string( store_id );
                game_object->script_flags[ flag_name ] = 1;
                std::string tips = std::string( "解锁商店:" ) + ( game_object->stores[ store_id ] ).store_name;
                set_tips( game_object , tips );
                return 0;
            }
        );
        //void lock_store( number store_id )
        lua_register( script_engines , "lock_store" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 1 )
                {
                    g_log( "lua_lock_store" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 1 );
                std::uint32_t store_id = luaL_checkinteger( L , 1 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                game_object->stores[ store_id ].usability = false;
                std::string flag_name = std::string( "store_" ) + std::to_string( store_id );
                game_object->script_flags.erase( flag_name );
                std::string tips = std::string( "锁定商店:" ) + ( game_object->stores[ store_id ] ).store_name;
                set_tips( game_object , tips );
                return 0;
            }
        );
        //void move_hero( number floor , number x , number y )
        lua_register( script_engines , "move_hero" ,
            []( lua_State * L ) -> int
            {
                int argument_count = lua_gettop( L );
                if ( argument_count < 3 )
                {
                    g_log( "lua_move_hero" , G_LOG_LEVEL_WARNING , "expecting exactly 3 arguments" );
                    return 0;
                }
                //discard any extra arguments passed
                lua_settop( L , 3 );
                std::uint32_t floor = luaL_checkinteger( L , 1 );
                std::uint32_t x = luaL_checkinteger( L , 2 );
                std::uint32_t y = luaL_checkinteger( L , 3 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 4 );
                
                move_hero( game_object , { x , y , floor } );
                return 0;
            }
        );
        //void game_win( void )
        lua_register( script_engines , "game_win" ,
            []( lua_State * L ) -> int
            {
                //arguments number impossible less than 0,don't need check
                //discard any extra arguments passed
                lua_settop( L , 0 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );
                
                game_win( game_object );
                return 0;
            }
        );
        //void game_lose( void )
        lua_register( script_engines , "game_lose" ,
            []( lua_State * L ) -> int
            {
                //arguments number impossible less than 0,don't need check
                //discard any extra arguments passed
                lua_settop( L , 0 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );
                
                game_lose( game_object );
                return 0;
            }
        );
    }

    std::vector<TowerGridLocation> find_path( GameEnvironment * game_object , TowerGridLocation goal )
    {
        Hero& hero = game_object->hero;
        TowerMap& tower = game_object->game_map;
        TowerGridLocation start = { hero.x , hero.y };
        if ( start == goal )
            return {};
        auto goal_grid = tower.get_grid( hero.floors , goal.x , goal.y );
        if ( goal_grid.type == GRID_TYPE::BOUNDARY )
        {
            return {};
        }
        if ( goal_grid.type == GRID_TYPE::WALL )
        {
            return {};
        }

        std::uint32_t length = tower.map[hero.floors].length;
        std::uint32_t width = tower.map[hero.floors].width;
        GridWithWeights game_map( length , width );

        for( std::uint32_t y = 0 ; y < length ; y++ )
        {
            for ( std::uint32_t x = 0 ; x < width ; x++ )
            {
                auto grid = tower.get_grid( hero.floors , x , y );
                switch ( grid.type )
                {
                    case GRID_TYPE::BOUNDARY:
                    case GRID_TYPE::WALL:
                    //case GRID_TYPE::NPC:
                        game_map.walls.insert( { std::int64_t( x ) , std::int64_t( y ) } );
                        break;
                    case GRID_TYPE::FLOOR:
                    case GRID_TYPE::ITEM:
                        break;
                    default :
                        game_map.forests.insert( { std::int64_t( x ) , std::int64_t( y ) } );
                }
            }
        }
        std::map<TowerGridLocation, TowerGridLocation> came_from;
        std::map<TowerGridLocation, double> cost_so_far;
        a_star_search( game_map , start , goal , came_from , cost_so_far );
        std::vector<TowerGridLocation> path = reconstruct_path( start , goal , came_from );
        
        return path;
    }

    bool open_door( GameEnvironment * game_object , event_position_t position )
    {
        Hero& hero = game_object->hero;
        TowerMap& tower = game_object->game_map;
        auto grid = tower.get_grid( std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
        if ( grid.type != GRID_TYPE::DOOR )
            return false;

        switch( grid.id )
        {
            case 1:
            {
                if ( hero.yellow_key >= 1 )
                {
                    set_grid_type( game_object , position );
                    hero.yellow_key--;
                }
                break;
            }
            case 2:
            {
                if ( hero.blue_key >= 1 )
                {
                    set_grid_type( game_object , position );
                    hero.blue_key--;
                }
                break;
            }
            case 3:
            {
                if ( hero.red_key >= 1 )
                {
                    set_grid_type( game_object , position );
                    hero.red_key--;
                }
                break;
            }
            default :
            {
                break;
            }
        }

        return false;
    }

    bool check_grid_type( GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id )
    {
        auto grid = game_object->game_map.get_grid( std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
        return ( grid.type == type_id );
    }

    void save_game( GameEnvironment * game_object , size_t save_id )
    {
        Glib::RefPtr<Gio::File> save_dir = Gio::File::create_for_path( DATABSE_RESOURCES_PATH );
        try
        {
            save_dir->make_directory_with_parents();
        }
        catch( const Glib::Error& e )
        {
            //if dir exists,do nothing.
            g_log( __func__ , G_LOG_LEVEL_WARNING , "%s" , e.what().c_str() );
        }

        std::string db_name = std::string( DATABSE_RESOURCES_PATH ) + std::to_string( save_id ) + std::string( ".db" );
        std::string fail_tips = std::string( "保存存档:" ) + std::to_string( save_id ) + std::string( "失败" );
        try
        {
            DataBase db( db_name );
            db.set_tower_info( game_object->game_map );
            db.set_hero_info( game_object->hero , 0 );
            db.set_script_flags( game_object->script_flags );
        }
        catch ( ... )
        {
            set_tips( game_object , fail_tips );
            return ;
        }
        std::string tips = std::string( "保存存档:" ) + std::to_string( save_id ) + std::string( "成功" );
        set_tips( game_object , tips );
    }

    void load_game( GameEnvironment * game_object , size_t save_id )
    {
        std::string db_name = std::string( DATABSE_RESOURCES_PATH ) + std::to_string( save_id ) + std::string( ".db" );
        std::string fail_tips = std::string( "读取存档:" ) + std::to_string( save_id ) + std::string( "失败" );
        Glib::RefPtr<Gio::File> db_file = Gio::File::create_for_path( db_name );
        if ( db_file->query_exists() == false )
        {
            fail_tips = std::string( "存档:" ) + std::to_string( save_id ) + std::string( "不存在" );
            set_tips( game_object , fail_tips );
            return ;
        }
        //try
        //{
            DataBase db( db_name );
            game_object->game_map = db.get_tower_info();
            game_object->hero = db.get_hero_info( 0 );
            game_object->script_flags = db.get_script_flags();

            //store unlock flag
            for ( auto& store : game_object->stores )
            {
                std::string flag_name = std::string( "store_" ) + std::to_string( store.first );
                if ( game_object->script_flags.find( flag_name ) != game_object->script_flags.end() )
                {
                    store.second.usability = true;
                }
                else
                {
                    store.second.usability = false;
                }
                
            }

            //floor jump unlock flag
            for ( std::uint32_t i = 0 ; i < game_object->game_map.map.size() ; i++ )
            {
                std::string floor_flag = std::string( "floors_" ) + std::to_string( i );
                if ( game_object->script_flags.find( floor_flag ) != game_object->script_flags.end() )
                {
                    game_object->access_floor[i] = true;
                }
                else
                {
                    game_object->access_floor[i] = false;
                }
            }
        //}
        //catch ( ... )
        //{
        //    set_tips( game_object , fail_tips );
        //    return ;
        //}
        std::string tips = std::string( "读取存档:" ) + std::to_string( save_id ) + std::string( "成功" );
        set_tips( game_object , tips );
    }

    void set_tips( GameEnvironment * game_object , std::string tips_content )
    {
        game_object->tips_content.push_back( tips_content );
        Glib::signal_timeout().connect_once( sigc::bind( &remove_tips , game_object ) , 1000 );
    }

    void set_grid_type( GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id )
    {
        TowerGrid target_grid = { type_id , static_cast<std::uint32_t>( 1 ) };
        game_object->game_map.set_grid( std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) , target_grid );
    }

    bool move_hero( GameEnvironment * game_object , event_position_t position )
    {
        if ( game_object->game_status == GAME_STATUS::FIND_PATH )
        {
            game_object->game_status = GAME_STATUS::NORMAL;
        }
        Hero& hero = game_object->hero;
        hero.x = std::get<0>( position );
        hero.y = std::get<1>( position );
        hero.floors = std::get<2>( position );
        return true;
    }

    std::int64_t get_combat_damage( GameEnvironment * game_object , std::uint32_t monster_id )
    {
        if ( game_object->monsters.find( monster_id ) == game_object->monsters.end() )
        {
            return -1;
        }
        Hero& hero = game_object->hero;
        Monster& monster = game_object->monsters[ monster_id ];
        switch( static_cast< std::uint32_t >( monster.type ) )
        {
            case static_cast<std::uint32_t>( ATTACK_TYPE::FIRST_ATTACK ):
                return get_combat_damage_of_first( hero , monster );
            case static_cast<std::uint32_t>( ATTACK_TYPE::NORMAL_ATTACK ):
                return get_combat_damage_of_normal( hero , monster );
            case static_cast<std::uint32_t>( ATTACK_TYPE::LAST_ATTACK ):
                return get_combat_damage_of_last( hero , monster );
            case static_cast<std::uint32_t>( ATTACK_TYPE::DOUBLE_ATTACK ):
                return get_combat_damage_of_double( hero , monster );
            case static_cast<std::uint32_t>( ATTACK_TYPE::EXTRA_QUOTA_DAMAGE ):
            {
                auto damage = get_combat_damage_of_normal( hero , monster );
                if ( damage < 0 )
                    return damage;
                else
                    return damage + monster.type_value;
            }
            case static_cast<std::uint32_t>( ATTACK_TYPE::EXTRA_PERCENT_DAMAG ):
            {
                auto damage = get_combat_damage_of_normal( hero , monster );
                //avoid var/0
                if ( monster.type_value == 0 )
                    return damage;
                if ( damage < 0 )
                    return damage;
                else
                    return damage + hero.life/monster.type_value;
            }
            default:
                break;
        }
        return -1;
    }

    bool battle( GameEnvironment * game_object , std::uint32_t monster_id )
    {
        if ( game_object->monsters.find( monster_id ) == game_object->monsters.end() )
            return false;
        Hero& hero = game_object->hero;
        Monster& monster = game_object->monsters[ monster_id ];
        std::int64_t damage = get_combat_damage( game_object , monster_id );
        if ( ( damage < 0 ) || ( damage >= hero.life ) )
            return false;
        hero.life =  hero.life - damage;
        hero.gold = hero.gold + monster.gold;
        hero.experience = hero.experience + monster.experience;
        std::string tips = std::string( "击杀:" ) + monster.name + std::string( ",获取金币:" ) + std::to_string( monster.gold )
            + std::string( ",经验:" ) + std::to_string( monster.experience );
        set_tips( game_object , tips );
        return true;
    }

    bool get_item( GameEnvironment * game_object , std::uint32_t item_id )
    {
        if ( game_object->items.find( item_id ) == game_object->items.end() )
            return false;
        Item& item = game_object->items[ item_id ];

        luaL_dostring( game_object->script_engines.get() , item.item_func.data() );
        std::string tips = item.item_detail;
        set_tips( game_object , tips );
        return true;
    }

    bool change_floor( GameEnvironment * game_object , std::uint32_t stair_id )
    {
        if ( game_object->stairs.find( stair_id ) == game_object->stairs.end() )
            return false;
        Stairs stair = game_object->stairs[ stair_id ];
        TowerMap& tower = game_object->game_map;

        if ( tower.map.find( stair.floors ) == tower.map.end() )
            return false;
        if ( stair.x >= tower.map[stair.floors].length )
            return false;
        if ( stair.y >= tower.map[stair.floors].width )
            return false;

        game_object->hero.floors = stair.floors;
        game_object->hero.x = stair.x;
        game_object->hero.y = stair.y;

        game_object->access_floor[ stair.floors ] = true;
        std::string floor_flag = std::string( "floors_" ) + std::to_string( stair.floors );
        game_object->script_flags[floor_flag] = 1;

        return true;
    }

    bool trigger_collision_event( GameEnvironment * game_object )
    {
        bool state = false;
        Hero& hero = game_object->hero;
        TowerMap& tower = game_object->game_map;

        if ( tower.map.find( hero.floors ) == tower.map.end() )
            return false;
        if ( hero.x >= tower.map[hero.floors].length )
            return false;
        if ( hero.y >= tower.map[hero.floors].width )
            return false;

        std::string script_name = CUSTOM_SCRIPTS_PATH"F" + std::to_string( ( game_object->hero ).floors ) + "_" + std::to_string( ( game_object->hero ).x ) + "_" + std::to_string( ( game_object->hero ).y ) + ".lua";
        if ( Glib::file_test( script_name , Glib::FILE_TEST_EXISTS ) )
        {
            if ( luaL_dofile( game_object->script_engines.get() , script_name.data() ) )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , lua_tostring( game_object->script_engines.get() , -1 ) );
            }
        }

        auto grid = tower.get_grid( hero.floors , hero.x , hero.y );
        switch( grid.type )
        {
            case GRID_TYPE::FLOOR:
            {
                state = true;
                break;
            }
            case GRID_TYPE::STAIRS:
            {
                state = change_floor( game_object , grid.id );
                break;
            }
            case GRID_TYPE::DOOR:
            {
                open_door( game_object , { hero.x , hero.y , hero.floors } );
                state = false;
                break;
            }
            case GRID_TYPE::NPC:
            {
                state = false;
                break;
            }
            case GRID_TYPE::MONSTER:
            {
                state = battle( game_object , grid.id );
                if ( state == true )
                    set_grid_type( game_object , { hero.x , hero.y , hero.floors } );
                else
                {
                    game_lose( game_object );
                }
                
                state = false;
                break;
            }
            case GRID_TYPE::ITEM:
            {
                state = get_item( game_object , grid.id );
                if ( state == true )
                    set_grid_type( game_object , { hero.x , hero.y , hero.floors } );
                break;
            }
            default :
            {
                state = false;
                break;
            }
        }

        script_name = CUSTOM_SCRIPTS_PATH"L" + std::to_string( ( game_object->hero ).floors ) + "_" + std::to_string( ( game_object->hero ).x ) + "_" + std::to_string( ( game_object->hero ).y ) + ".lua";
        if ( Glib::file_test( script_name , Glib::FILE_TEST_EXISTS ) )
        {
            if ( luaL_dofile( game_object->script_engines.get() , script_name.data() ) )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , lua_tostring( game_object->script_engines.get() , -1 ) );
            }
        }

        return state;
    }

    void background_music_switch( GameEnvironment * game_object )
    {
        static bool play_music = true;
        play_music = !play_music;

        if ( play_music == true )
            game_object->music.play_resume();
        else
            game_object->music.play_pause();
    }

    void path_line_switch( GameEnvironment * game_object )
    {
        game_object->draw_path = !game_object->draw_path;
    }

    void open_floor_jump( GameEnvironment * game_object )
    {
        switch ( game_object->game_status )
        {
            case GAME_STATUS::GAME_LOSE:
            case GAME_STATUS::GAME_WIN:
            case GAME_STATUS::GAME_MENU:
            case GAME_STATUS::STORE_MENU:
            case GAME_STATUS::JUMP_MENU:
                return ;
            default:
                break;
        }

        if ( game_object->floors_jump.find( game_object->hero.floors ) == game_object->floors_jump.end() )
        {
            set_tips( game_object , "这层楼禁止使用楼层跳跃器！" );
            return ;
        }

        game_object->game_status = GAME_STATUS::JUMP_MENU;
        game_object->focus_item_id = 0;
        temp_pos= { game_object->hero.floors , game_object->hero.x , game_object->hero.y };
        set_jump_menu( game_object );
    }

    void close_floor_jump( GameEnvironment * game_object )
    {
        if ( game_object->game_status != GAME_STATUS::JUMP_MENU )
            return ;
        game_object->game_status = GAME_STATUS::NORMAL;
    }

    void open_store_menu_v2( GameEnvironment * game_object )
    {
        switch ( game_object->game_status )
        {
            case GAME_STATUS::GAME_LOSE:
            case GAME_STATUS::GAME_WIN:
            case GAME_STATUS::GAME_MENU:
            case GAME_STATUS::STORE_MENU:
            case GAME_STATUS::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_object->game_status = GAME_STATUS::STORE_MENU;
        game_object->focus_item_id = 0;
        set_store_menu( game_object );
    }

    void close_store_menu_v2( GameEnvironment * game_object )
    {
        if ( game_object->game_status != GAME_STATUS::STORE_MENU )
            return ;
        game_object->game_status = GAME_STATUS::NORMAL;
    }

    void open_start_menu( GameEnvironment * game_object )
    {
        switch ( game_object->game_status )
        {
            case GAME_STATUS::GAME_LOSE:
            case GAME_STATUS::GAME_WIN:
                break;
            default:
                return ;
        }

        game_object->game_status = GAME_STATUS::START_MENU;
        game_object->focus_item_id = 0;
        set_start_menu( game_object );
    }

    void open_game_menu_v2( GameEnvironment * game_object )
    {
        switch ( game_object->game_status )
        {
            case GAME_STATUS::GAME_LOSE:
            case GAME_STATUS::GAME_WIN:
            case GAME_STATUS::GAME_MENU:
            case GAME_STATUS::STORE_MENU:
            case GAME_STATUS::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_object->game_status = GAME_STATUS::GAME_MENU;
        game_object->focus_item_id = 0;
        set_game_menu( game_object );
    }

    void close_game_menu_v2( GameEnvironment * game_object )
    {
        if ( game_object->game_status != GAME_STATUS::GAME_MENU )
            return ;
        game_object->game_status = GAME_STATUS::NORMAL;
    }

    void game_win( GameEnvironment * game_object )
    {
        switch ( game_object->game_status )
        {
            case GAME_STATUS::GAME_LOSE:
            case GAME_STATUS::GAME_WIN:
            case GAME_STATUS::GAME_MENU:
            case GAME_STATUS::STORE_MENU:
            case GAME_STATUS::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_object->game_message.push_back( "恭喜通关" );
        game_object->game_message.push_back(
            std::string( "你的得分为:" ) + std::to_string(
                game_object->hero.life + ( game_object->hero.attack +
                game_object->hero.defense )*10 + ( game_object->hero.level )*100
            )
        );
        game_object->game_status = GAME_STATUS::GAME_WIN;
    }

    void game_lose( GameEnvironment * game_object )
    {
        switch ( game_object->game_status )
        {
            case GAME_STATUS::GAME_LOSE:
            case GAME_STATUS::GAME_WIN:
            case GAME_STATUS::GAME_MENU:
            case GAME_STATUS::STORE_MENU:
            case GAME_STATUS::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_object->game_message.push_back( "游戏失败" );
        game_object->game_message.push_back(
            std::string( "你的得分为:" ) + std::to_string(
                game_object->hero.life + ( game_object->hero.attack +
                game_object->hero.defense )*( game_object->hero.level )
            )
        );
        game_object->game_status = GAME_STATUS::GAME_LOSE;
    }

    void exit_game( GameEnvironment * game_object )
    {
        game_object->game_status = GAME_STATUS::GAME_END;
    }

    static std::int64_t get_combat_damage_of_last( const Hero& hero , const Monster& monster )
    {
        //if don't cast to int64_t when attack < defense the result underflow to UINT32_MAX - result
        std::int64_t hero_combat_damage = static_cast<std::int64_t>( hero.attack ) - monster.defense;
        std::int64_t monster_combat_damage = static_cast<std::int64_t>( monster.attack ) - hero.defense;

        if ( hero_combat_damage <= 0 )
            return -1;
        if ( monster_combat_damage <= 0 )
            return 0;

        std::uint32_t hero_combat_count;

        if ( monster.life%hero_combat_damage != 0 )
            hero_combat_count = monster.life/hero_combat_damage + 1;
        else
            hero_combat_count = monster.life/hero_combat_damage;

        //damage result overflow,return -1 avoid this;
        if ( ( hero_combat_count - 1 ) >= INT64_MAX/monster_combat_damage )
            return -1;
        //hero first attack,monster dead can't attack
        return ( hero_combat_count - 1 )*monster_combat_damage;
    }

    static std::int64_t get_combat_damage_of_normal( const Hero& hero , const Monster& monster )
    {
        if ( hero.level > monster.level )
            return get_combat_damage_of_last( hero , monster );
        else
            return get_combat_damage_of_first( hero , monster );
        return -1;
    }

    static std::int64_t get_combat_damage_of_first( const Hero& hero , const Monster& monster )
    {
        //if don't cast to int64_t when attack < defense the result underflow to UINT32_MAX - result
        std::int64_t hero_combat_damage = static_cast<std::int64_t>( hero.attack ) - monster.defense;
        std::int64_t monster_combat_damage = static_cast<std::int64_t>( monster.attack ) - hero.defense;
        if ( hero_combat_damage <= 0 )
            return -1;
        if ( monster_combat_damage <= 0 )
            return 0;

        std::uint32_t hero_combat_count; 
        if ( monster.life%hero_combat_damage != 0 )
            hero_combat_count = monster.life/hero_combat_damage + 1;
        else
            hero_combat_count = monster.life/hero_combat_damage;

        //damage result overflow,return -1 avoid this;
        if ( hero_combat_count >= INT64_MAX/monster_combat_damage )
            return -1;
        return hero_combat_count*monster_combat_damage;
    }

    static std::int64_t get_combat_damage_of_double( const Hero& hero , const Monster& monster )
    {
        //if don't cast to int64_t when attack < defense the result underflow to UINT32_MAX - result
        std::int64_t hero_combat_damage = static_cast<std::int64_t>( hero.attack ) - monster.defense;
        std::int64_t monster_combat_damage = static_cast<std::int64_t>( monster.attack ) - hero.defense;
        if ( hero_combat_damage <= 0 )
            return -1;
        if ( monster_combat_damage <= 0 )
            return 0;
        std::uint32_t hero_combat_count; 
        if ( monster.life%hero_combat_damage != 0 )
            hero_combat_count = monster.life/hero_combat_damage + 1;
        else
            hero_combat_count = monster.life/hero_combat_damage;
        if ( hero.level > monster.level )
        {
            //damage result overflow,return -1 avoid this;
            if ( ( hero_combat_count - 1 ) >= INT64_MAX/( monster_combat_damage*2 ) )
                return -1;
            //hero first attack,monster dead can't attack
            return ( hero_combat_count - 1 )*monster_combat_damage*2;
        }
        else
        {
            //damage result overflow,return -1 avoid this;
            if ( hero_combat_count >= INT64_MAX/( monster_combat_damage*2 ) )
                return -1;
            //hero first attack,monster dead can't attack
            return hero_combat_count*monster_combat_damage*2;
        }
        return -1;
    }

    void back_jump( GameEnvironment * game_object )
    {
        game_object->hero.floors = std::get<0>( temp_pos );
        game_object->hero.x = std::get<1>( temp_pos );
        game_object->hero.y = std::get<2>( temp_pos );
    }

    static bool try_jump( GameEnvironment * game_object )
    {
        try
        {
            std::uint32_t x = game_object->floors_jump.at( game_object->hero.floors ).first;
            std::uint32_t y = game_object->floors_jump.at( game_object->hero.floors ).second;
            game_object->hero.x = x;
            game_object->hero.y = y;
            return true;
        }
        catch(const std::out_of_range& e)
        {
            std::string tips = std::string( "该层禁止跃入" );
            set_tips( game_object , tips );
            return false;
        }
    }

    static void set_jump_menu( GameEnvironment * game_object )
    {
        game_object->menu_items.clear();
        game_object->menu_items.push_back({
            [](){ return std::string( "最上层" ); },
            [ game_object ](){
                auto end_iter = game_object->game_map.map.end();
                game_object->hero.floors = ( std::prev( end_iter ) )->first;
                try_jump( game_object );
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "上一层" ); },
            [ game_object ](){
                auto current_iter = game_object->game_map.map.find( game_object->hero.floors );
                if ( ( current_iter = std::next( current_iter ) ) != game_object->game_map.map.end() )
                {
                    game_object->hero.floors = current_iter->first;
                    try_jump( game_object );
                }
                else
                {
                    std::string tips = std::string( "已是最上层" );
                    set_tips( game_object , tips );
                }
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "下一层" ); },
            [ game_object ](){
                auto current_iter = game_object->game_map.map.find( game_object->hero.floors );
                if ( current_iter != game_object->game_map.map.begin() )
                {
                    current_iter = std::prev( current_iter );
                    game_object->hero.floors = current_iter->first;
                    try_jump( game_object );
                }
                else
                {
                    std::string tips = std::string( "已是最下层" );
                    set_tips( game_object , tips );
                }
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "最下层" ); },
            [ game_object ](){
                auto begin_iter = game_object->game_map.map.begin();
                game_object->hero.floors = begin_iter->first;
                try_jump( game_object );
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "确定跳跃" ); },
            [ game_object ](){
                try
                {
                    game_object->floors_jump.at( std::get<0>( temp_pos ) );
                }
                catch(const std::out_of_range& e)
                {
                    std::string tips = std::string( "所在层禁止跳跃楼层" );
                    set_tips( game_object , tips );
                    back_jump( game_object );
                    return ;
                }
                if ( game_object->access_floor[ game_object->hero.floors ] == false )
                {
                    std::string tips = std::string( "所选择的楼层当前禁止跃入" );
                    set_tips( game_object , tips );
                    back_jump( game_object );
                    return ;
                }
                bool state = try_jump( game_object );
                if ( state == false )
                    back_jump( game_object );
                close_floor_jump( game_object );
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "取消跳跃" ); },
            [ game_object ](){
                back_jump( game_object );
                close_floor_jump( game_object );
            }
        });
    }

    static void set_start_menu( GameEnvironment * game_object )
    {
        game_object->menu_items.clear();
        game_object->menu_items.push_back({
            [](){ return std::string( "重新游戏" ); },
            [ game_object ](){
                game_object->initial_gamedata();
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "读取存档" ); },
            [ game_object ](){
                load_game( game_object , 1 );
                game_object->game_status = GAME_STATUS::NORMAL;
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "退出游戏" ); },
            [ game_object ](){
                exit_game( game_object );
            }
        });
    }

    static void set_game_menu( GameEnvironment * game_object )
    {
        game_object->menu_items.clear();

        game_object->menu_items.push_back({
            [](){ return std::string( "保存存档" ); },
            [ game_object ](){ save_game( game_object , 1 ); }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "读取存档" ); },
            [ game_object ](){ load_game( game_object , 1 ); }
        });
        game_object->menu_items.push_back({
            [ game_object ](){
                if ( game_object->music.get_state() == PLAY_STATE::PLAYING )
                    return std::string( "背景音乐: 开" );
                else
                    return std::string( "背景音乐: 关" );
            },
            [ game_object ](){ background_music_switch( game_object ); }
        });
        game_object->menu_items.push_back({
            [ game_object ](){
                if ( game_object->draw_path == TRUE )
                    return std::string( "寻路指示: 开" );
                else
                    return std::string( "寻路指示: 关" );
            },
            [ game_object ](){ path_line_switch( game_object ); }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_object ](){ close_game_menu_v2( game_object ); }
        });
    }

    static void set_store_menu( GameEnvironment * game_object )
    {
        game_object->menu_items.clear();
        for ( auto& store : game_object->stores )
        {
            if ( store.second.usability != true )
            {
                continue;
            }

            std::string store_name = store.second.store_name;
            std::uint32_t store_id = store.first;
            game_object->menu_items.push_back({
                [ store_name ](){ return store_name; },
                [ game_object , store_id ](){ set_sub_store_menu( game_object , store_id ); }
            });
        }
        game_object->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_object ](){ close_store_menu_v2( game_object ); }
        });
    }

    static void set_sub_store_menu( GameEnvironment * game_object , std::uint32_t store_id )
    {
        game_object->focus_item_id = 0;

        game_object->menu_items.clear();
        game_object->menu_items.push_back({
            [](){ return std::string( "返回上级菜单" ); },
            [ game_object ](){ set_store_menu( game_object ); }
        });
        Store& store = game_object->stores[store_id];
        for ( auto& commoditie : store.commodities )
        {
            std::string commodity_detail = commoditie.first;
            std::string commodity_function = commoditie.second;
            game_object->menu_items.push_back({
                [ commodity_detail ](){ return commodity_detail; },
                [ game_object , commodity_function ](){ luaL_dostring( game_object->script_engines.get() , commodity_function.c_str() ); }
            });
        }

        game_object->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_object ](){ close_store_menu_v2( game_object ); }
        });
    }

    static void remove_tips( GameEnvironment * game_object )
    {
        if ( game_object->tips_content.empty() )
            return ;
        game_object->tips_content.pop_front();
    }
}
