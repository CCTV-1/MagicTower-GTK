#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cinttypes>

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
#include <jansson.h>

#include "game_event.h"
#include "env_var.h"
#include "resources.h"

namespace MagicTower
{
    //layer , x , y
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
    static void set_sub_store_menu( GameEnvironment * game_object , const char * item_content );
    static std::string deserialize_commodity_content( const char * content );
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
        //void set_grid_type( number layer , number x , number y , number grid_id )
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
                std::uint32_t layer = luaL_checkinteger( L , 1 );
                std::uint32_t x = luaL_checkinteger( L , 2 );
                std::uint32_t y = luaL_checkinteger( L , 3 );
                std::uint32_t grid_id = luaL_checkinteger( L , 4 );
                GRID_TYPE grid_type = static_cast<GRID_TYPE>( grid_id );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 5 );
                
                set_grid_type( game_object , { x , y , layer } , grid_type );
                return 0;
            }
        );
        //number get_grid_type( number layer , number x , number y )
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
                std::uint32_t layer = luaL_checkinteger( L , 1 );
                std::uint32_t x = luaL_checkinteger( L , 2 );
                std::uint32_t y = luaL_checkinteger( L , 3 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 4 );
                TowerGrid& grid = get_tower_grid( game_object->towers , x , y , layer );
                lua_pushnumber( L , grid.type );
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
                lua_newtable( L );
                lua_pushnumber( L , hero.layers );
                lua_setfield( L , -2 , "layers" );
                lua_pushnumber( L , hero.x );
                lua_setfield( L , -2 , "x" );
                lua_pushnumber( L , hero.y );
                lua_setfield( L , -2 , "y" );
                lua_pushnumber( L , hero.level );
                lua_setfield( L , -2 , "level" );
                lua_pushnumber( L , hero.life );
                lua_setfield( L , -2 , "life" );
                lua_pushnumber( L , hero.attack );
                lua_setfield( L , -2 , "attack" );
                lua_pushnumber( L , hero.defense );
                lua_setfield( L , -2 , "defense" );
                lua_pushnumber( L , hero.gold );
                lua_setfield( L , -2 , "gold" );
                lua_pushnumber( L , hero.experience );
                lua_setfield( L , -2 , "experience" );
                lua_pushnumber( L , hero.yellow_key );
                lua_setfield( L , -2 , "yellow_key" );
                lua_pushnumber( L , hero.blue_key );
                lua_setfield( L , -2 , "blue_key" );
                lua_pushnumber( L , hero.red_key );
                lua_setfield( L , -2 , "red_key" );
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
                luaL_checktype( L , 1 , LUA_TTABLE );
                lua_getfield( L , 1 , "layers" );
                lua_getfield( L , 1 , "x" );
                lua_getfield( L , 1 , "y" );
                lua_getfield( L , 1 , "level" );
                lua_getfield( L , 1 , "life" );
                lua_getfield( L , 1 , "attack" );
                lua_getfield( L , 1 , "defense" );
                lua_getfield( L , 1 , "gold" );
                lua_getfield( L , 1 , "experience" );
                lua_getfield( L , 1 , "yellow_key" );
                lua_getfield( L , 1 , "blue_key" );
                lua_getfield( L , 1 , "red_key" );
                game_object->hero.layers = luaL_checkinteger( L , -12 );
                game_object->hero.x = luaL_checkinteger( L , -11 );
                game_object->hero.y = luaL_checkinteger( L , -10 );
                game_object->hero.level = luaL_checkinteger( L , -9 );
                game_object->hero.life = luaL_checkinteger( L , -8 );
                game_object->hero.attack = luaL_checkinteger( L , -7 );
                game_object->hero.defense = luaL_checkinteger( L , -6 );
                game_object->hero.gold = luaL_checkinteger( L , -5 );
                game_object->hero.experience = luaL_checkinteger( L , -4 );
                game_object->hero.yellow_key = luaL_checkinteger( L , -3 );
                game_object->hero.blue_key = luaL_checkinteger( L , -2 );
                game_object->hero.red_key = luaL_checkinteger( L , -1 );
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
        //void open_menu( table of ( string item_name , string item_detail_json ) )
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
                game_object->store_list[ store_id ].usability = true;
                std::string tips = std::string( "解锁商店:" ) + ( game_object->store_list[ store_id ] ).name;
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
                game_object->store_list[ store_id ].usability = false;
                std::string tips = std::string( "锁定商店:" ) + ( game_object->store_list[ store_id ] ).name;
                set_tips( game_object , tips );
                return 0;
            }
        );
        //void move_hero( number layer , number x , number y )
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
                std::uint32_t layer = luaL_checkinteger( L , 1 );
                std::uint32_t x = luaL_checkinteger( L , 2 );
                std::uint32_t y = luaL_checkinteger( L , 3 );
                lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 4 );
                
                move_hero( game_object , { x , y , layer } );
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
        Tower& tower = game_object->towers;
        TowerGridLocation start = { hero.x , hero.y };
        if ( start == goal )
            return {};
        auto goal_grid = get_tower_grid( tower , hero.layers , goal.x , goal.y );
        if ( goal_grid.type == MagicTower::GRID_TYPE::BOUNDARY )
        {
            return {};
        }
        if ( goal_grid.type == MagicTower::GRID_TYPE::WALL )
        {
            return {};
        }
        
        GridWithWeights game_map( tower.LENGTH , tower.WIDTH );

        for( size_t y = 0 ; y < tower.LENGTH ; y++ )
        {
            for ( size_t x = 0 ; x < tower.WIDTH ; x++ )
            {
                auto grid = get_tower_grid( tower , hero.layers , x , y );
                switch ( grid.type )
                {
                    case MagicTower::GRID_TYPE::BOUNDARY:
                    case MagicTower::GRID_TYPE::WALL:
                    //case MagicTower::GRID_TYPE::NPC:
                        game_map.walls.insert( { std::int64_t( x ) , std::int64_t( y ) } );
                        break;
                    case MagicTower::GRID_TYPE::FLOOR:
                    case MagicTower::GRID_TYPE::ITEM:
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
        Tower& tower = game_object->towers;
        auto grid = get_tower_grid( tower , std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
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
        auto grid = get_tower_grid( game_object->towers , std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
        return ( grid.id == type_id );
    }

    void save_game( GameEnvironment * game_object , size_t save_id )
    {
        constexpr const char * save_path = "./save/";
        Glib::RefPtr<Gio::File> save_dir = Gio::File::create_for_path( save_path );
        try
        {
            save_dir->make_directory_with_parents();
        }
        catch( const Glib::Error& e )
        {
            //if dir exists,do nothing.
            g_log( __func__ , G_LOG_LEVEL_WARNING , "%s" , e.what().c_str() );
        }

        std::string db_name = std::string( save_path ) + std::to_string( save_id ) + std::string( ".db" );
        std::string fail_tips = std::string( "保存存档:" ) + std::to_string( save_id ) + std::string( "失败" );
        try
        {
            MagicTower::DataBase db( db_name );
            db.set_tower_info( game_object->towers , 0 );
            db.set_hero_info( game_object->hero , 0 );
            db.set_stairs_list( game_object->stairs );
            db.set_store_list( game_object->store_list );
            db.set_monster_list( game_object->monsters );
            db.set_access_layers( game_object->access_layer );
            db.set_jump_map( game_object->layers_jump );
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
        std::string db_name = std::string( "./save/" ) + std::to_string( save_id ) + std::string( ".db" );
        std::string fail_tips = std::string( "读取存档:" ) + std::to_string( save_id ) + std::string( "失败" );
        Glib::RefPtr<Gio::File> db_file = Gio::File::create_for_path( db_name );
        if ( db_file->query_exists() == false )
        {
            fail_tips = std::string( "存档:" ) + std::to_string( save_id ) + std::string( "不存在" );
            set_tips( game_object , fail_tips );
            return ;
        }
        try
        {
            MagicTower::DataBase db( db_name );
            game_object->towers = db.get_tower_info( 0 );
            game_object->hero = db.get_hero_info( 0 );
            game_object->stairs = db.get_stairs_list();
            game_object->store_list = db.get_store_list();
            game_object->monsters = db.get_monster_list();
            game_object->access_layer = db.get_access_layers();
            game_object->layers_jump = db.get_jump_map();
            game_object->script_flags = db.get_script_flags();
        }
        catch ( ... )
        {
            set_tips( game_object , fail_tips );
            return ;
        }
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
        set_tower_grid( game_object->towers , target_grid , std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
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
        hero.layers = std::get<2>( position );
        return true;
    }

    std::int64_t get_combat_damage( GameEnvironment * game_object , std::uint32_t monster_id )
    {
        if ( monster_id - 1 >= game_object->monsters.size() )
        {
            return -1;
        }
        Hero& hero = game_object->hero;
        Monster& monster = game_object->monsters[ monster_id - 1 ];
        switch( static_cast< std::uint32_t >( monster.type ) )
        {
            case ATTACK_TYPE::FIRST_ATTACK:
                return get_combat_damage_of_first( hero , monster );
            case ATTACK_TYPE::NORMAL_ATTACK:
                return get_combat_damage_of_normal( hero , monster );
            case ATTACK_TYPE::LAST_ATTACK:
                return get_combat_damage_of_last( hero , monster );
            case ATTACK_TYPE::DOUBLE_ATTACK:
                return get_combat_damage_of_double( hero , monster );
            case ATTACK_TYPE::EXTRA_QUOTA_DAMAGE:
            {
                auto damage = get_combat_damage_of_normal( hero , monster );
                if ( damage < 0 )
                    return damage;
                else
                    return damage + monster.type_value;
            }
            case ATTACK_TYPE::EXTRA_PERCENT_DAMAG:
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
        if ( monster_id - 1 >= game_object->monsters.size() )
            return false;
        Hero& hero = game_object->hero;
        Monster& monster = game_object->monsters[ monster_id - 1 ];
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

    bool get_item( GameEnvironment * game_object , std::uint32_t commodity_id )
    {
        if ( commodity_id - 1 >= game_object->items.size() )
            return false;
        if ( game_object->items.find( commodity_id ) == game_object->items.end() )
            return false;
        Item& item = game_object->items[ commodity_id ];

        luaL_dostring( game_object->script_engines.get() , item.item_func.data() );
        std::string tips = item.item_detail;
        set_tips( game_object , tips );
        return true;
    }

    bool change_floor( GameEnvironment * game_object , std::uint32_t stair_id )
    {
        if ( stair_id - 1 >= game_object->stairs.size() )
            return false;
        Stairs stair = game_object->stairs[ stair_id - 1 ];
        Tower& tower = game_object->towers;
        if ( stair.layers >= tower.HEIGHT )
            return false;
        if ( stair.x >= tower.LENGTH )
            return false;
        if ( stair.y >= tower.WIDTH )
            return false;

        game_object->hero.layers = stair.layers;
        game_object->hero.x = stair.x;
        game_object->hero.y = stair.y;

        game_object->access_layer[ stair.layers ] = true;

        return true;
    }

    bool trigger_collision_event( GameEnvironment * game_object )
    {
        bool state = false;
        Hero& hero = game_object->hero;
        Tower& tower = game_object->towers;

        if ( hero.x >= tower.LENGTH )
            return false;
        if ( hero.y >= tower.WIDTH )
            return false;
        if ( hero.layers >= tower.HEIGHT )
            return false;

        std::string script_name = CUSTOM_SCRIPTS_PATH"F" + std::to_string( ( game_object->hero ).layers ) + "_" + std::to_string( ( game_object->hero ).x ) + "_" + std::to_string( ( game_object->hero ).y ) + ".lua";
        if ( Glib::file_test( script_name , Glib::FILE_TEST_EXISTS ) )
        {
            if ( luaL_dofile( game_object->script_engines.get() , script_name.data() ) )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , lua_tostring( game_object->script_engines.get() , -1 ) );
            }
        }

        auto grid = get_tower_grid( tower , hero.layers , hero.x , hero.y );
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
                open_door( game_object , { hero.x , hero.y , hero.layers } );
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
                    set_grid_type( game_object , { hero.x , hero.y , hero.layers } );
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
                    set_grid_type( game_object , { hero.x , hero.y , hero.layers } );
                break;
            }
            default :
            {
                state = false;
                break;
            }
        }

        script_name = CUSTOM_SCRIPTS_PATH"L" + std::to_string( ( game_object->hero ).layers ) + "_" + std::to_string( ( game_object->hero ).x ) + "_" + std::to_string( ( game_object->hero ).y ) + ".lua";
        if ( Glib::file_test( script_name , Glib::FILE_TEST_EXISTS ) )
        {
            if ( luaL_dofile( game_object->script_engines.get() , script_name.data() ) )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , lua_tostring( game_object->script_engines.get() , -1 ) );
            }
        }

        return state;
    }

    bool shopping( GameEnvironment * game_object , const char * commodity_json )
    {
        json_error_t json_error;
        json_t * root = json_loads( commodity_json , 0 , &json_error );
        if ( root == nullptr )
        {
            json_decref( root );
            return false;
        }

        json_t * price_type_node = json_object_get( root , "price_type" );
        if ( !json_is_string( price_type_node ) )
        {
            json_decref( root );
            return false;
        }
        std::string price_type( json_string_value( price_type_node ) );
        json_t * price_node = json_object_get( root , "price" );
        if ( !json_is_integer( price_node ) )
        {
            json_decref( root );
            return false;
        }
        json_int_t price_value = json_integer_value( price_node );
        json_t * commodity_type_node = json_object_get( root , "commodity_type" );
        if ( !json_is_string( commodity_type_node ) )
        {
            json_decref( root );
            return false;
        }
        std::string commodity_type( json_string_value( commodity_type_node ) );
        json_t * commodity_value_node = json_object_get( root , "commodity_value" );
        if ( !json_is_integer( commodity_value_node ) )
        {
            json_decref( root );
            return false;
        }
        json_int_t commodity_value = json_integer_value( commodity_value_node );
        if ( price_type == std::string( "LEVEL" ) )
        {
            if ( game_object->hero.level >= price_value )
                game_object->hero.level -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "LIFE" ) )
        {
            if ( game_object->hero.life >= price_value )
                game_object->hero.life -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "ATTACK" ) )
        {
            if ( game_object->hero.attack >= price_value )
                game_object->hero.attack -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "DEFENCE" ) )
        {
            if ( game_object->hero.defense >= price_value )
                game_object->hero.defense -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "GOLD" ) )
        {
            if ( game_object->hero.gold >= price_value )
                game_object->hero.gold -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "EXPERIENCE" ) )
        {
            if ( game_object->hero.experience >= price_value )
                game_object->hero.experience -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "YELLOW_KEY" ) )
        {
            if ( game_object->hero.yellow_key >= price_value )
                game_object->hero.yellow_key -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "BLUE_KEY" ) )
        {
            if ( game_object->hero.blue_key >= price_value )
                game_object->hero.blue_key -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "RED_KEY" ) )
        {
            if ( game_object->hero.red_key >= price_value )
                game_object->hero.red_key -= price_value;
            else
            {
                json_decref( root );
                return false;
            }
        }
        else if ( price_type == std::string( "ALL_KEY" ) )
        {
            if ( game_object->hero.yellow_key >= price_value &&
                 game_object->hero.blue_key   >= price_value &&
                 game_object->hero.red_key    >= price_value )
            {
                game_object->hero.yellow_key -= price_value;
                game_object->hero.blue_key -= price_value;
                game_object->hero.red_key -= price_value;
            }
            else
            {
                json_decref( root );
                return false;
            }
        }
        else
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unknown price type" );
            json_decref( root );
            return false;
        }
        
        if ( commodity_type == std::string( "CHANGE_LEVEL" ) )
        {
            game_object->hero.level  += commodity_value;
            game_object->hero.life  += 1000*commodity_value;
            game_object->hero.attack  += 7*commodity_value;
            game_object->hero.defense  += 7*commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_LIFE" ) )
        {
            game_object->hero.life  += commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_ATTACK" ) )
        {
            game_object->hero.attack  += commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_DEFENSE" ) )
        {
            game_object->hero.defense  += commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_GOLD" ) )
        {
            game_object->hero.gold  += commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_EXPERIENCE" ) )
        {
            game_object->hero.experience  += commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_YELLOW_KEY" ) )
        {
            game_object->hero.yellow_key  += commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_BLUE_KEY" ) )
        {
            game_object->hero.blue_key  += commodity_value;
        }
        else if ( commodity_type == std::string( "CHANGE_RED_KEY" ) )
        {
            game_object->hero.red_key  += commodity_value;
        }
        else
        {
            return false;
        }
        return true;
    }

    void do_shop( GameEnvironment * game_object , std::string shop_content )
    {
        luaL_dostring( game_object->script_engines.get() , shop_content.c_str() );
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

    void open_layer_jump( GameEnvironment * game_object )
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

        game_object->game_status = GAME_STATUS::JUMP_MENU;
        game_object->focus_item_id = 0;
        temp_pos= { game_object->hero.layers , game_object->hero.x , game_object->hero.y };
        set_jump_menu( game_object );
    }

    void close_layer_jump( GameEnvironment * game_object )
    {
        if ( game_object->game_status != GAME_STATUS::JUMP_MENU )
            return ;
        game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
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

        game_object->game_status = MagicTower::GAME_STATUS::STORE_MENU;
        game_object->focus_item_id = 0;
        set_store_menu( game_object );
    }

    void close_store_menu_v2( GameEnvironment * game_object )
    {
        if ( game_object->game_status != GAME_STATUS::STORE_MENU )
            return ;
        game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
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
        if ( game_object->game_status != MagicTower::GAME_STATUS::GAME_MENU )
            return ;
        game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
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
        game_object->hero.layers = std::get<0>( temp_pos );
        game_object->hero.x = std::get<1>( temp_pos );
        game_object->hero.y = std::get<2>( temp_pos );
    }

    static bool try_jump( GameEnvironment * game_object )
    {
        try
        {
            std::uint32_t x = game_object->layers_jump.at( game_object->hero.layers ).first;
            std::uint32_t y = game_object->layers_jump.at( game_object->hero.layers ).second;
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
                game_object->hero.layers = game_object->towers.HEIGHT - 1;
                try_jump( game_object );
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "上一层" ); },
            [ game_object ](){
                if ( game_object->hero.layers < game_object->towers.HEIGHT - 1 )
                {
                    game_object->hero.layers += 1;
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
                if ( game_object->hero.layers > 0 )
                {
                    game_object->hero.layers -= 1;
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
                game_object->hero.layers = 0;
                try_jump( game_object );
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "确定跳跃" ); },
            [ game_object ](){
                try
                {
                    game_object->layers_jump.at( std::get<0>( temp_pos ) );
                }
                catch(const std::out_of_range& e)
                {
                    std::string tips = std::string( "所在层禁止跳跃楼层" );
                    set_tips( game_object , tips );
                    back_jump( game_object );
                    return ;
                }
                if ( game_object->access_layer[ game_object->hero.layers ] == false )
                {
                    std::string tips = std::string( "所选择的楼层当前禁止跃入" );
                    set_tips( game_object , tips );
                    back_jump( game_object );
                    return ;
                }
                bool state = try_jump( game_object );
                if ( state == false )
                    back_jump( game_object );
                close_layer_jump( game_object );
            }
        });
        game_object->menu_items.push_back({
            [](){ return std::string( "取消跳跃" ); },
            [ game_object ](){
                back_jump( game_object );
                close_layer_jump( game_object );
            }
        });
    }

    static void set_start_menu( GameEnvironment * game_object )
    {
        game_object->menu_items.clear();
        game_object->menu_items.push_back({
            [](){ return std::string( "重新游戏" ); },
            [ game_object ](){
                MagicTower::DataBase db( DATABSE_RESOURCES_PATH );
                game_object->towers = db.get_tower_info( 0 );
                game_object->hero = db.get_hero_info( 0 );
                game_object->stairs = db.get_stairs_list();
                game_object->store_list = db.get_store_list();
                game_object->monsters = db.get_monster_list();
                //game_object->items = db.get_item_list();
                game_object->script_flags = db.get_script_flags();
                game_object->access_layer = db.get_access_layers();
                game_object->layers_jump = db.get_jump_map();
                game_object->game_status = GAME_STATUS::NORMAL;
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
        for ( auto store : game_object->store_list )
        {
            if ( store.usability != true )
            {
                continue;
            }

            game_object->menu_items.push_back({
                [ store ](){ return store.name; },
                [ game_object , store ](){ set_sub_store_menu( game_object , store.content.c_str() ); }
            });
        }
        game_object->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_object ](){ close_store_menu_v2( game_object ); }
        });
    }

    static void set_sub_store_menu( GameEnvironment * game_object , const char * item_content )
    {
        game_object->focus_item_id = 0;

        json_error_t json_error;
        json_t * root = json_loads( item_content , 0 , &json_error );
        if ( root == nullptr )
        {
            json_decref( root );
            return ;
        }

        //when call clear will free item_content content,so first call json_loads copy item_content content.
        game_object->menu_items.clear();
        json_t * commodity_list = json_object_get( root , "commoditys" );
        if ( !json_is_array( commodity_list ) )
        {
            json_decref( root );
            return ;
        }
        size_t commodity_size = json_array_size( commodity_list );
        game_object->menu_items.push_back({
            [](){ return std::string( "返回上级菜单" ); },
            [ game_object ](){ set_store_menu( game_object ); }
        });
        for( size_t i = 0 ; i < commodity_size ; i++ )
        {
            json_t * commodity_node = json_array_get( commodity_list , i );
            if ( !json_is_object( commodity_node ) )
            {
                json_decref( root );
                return ;
            }
            std::unique_ptr< char , decltype( &free ) > commodity_json( json_dumps( commodity_node , JSON_INDENT( 4 ) ) , free );
            std::string commodity_content( commodity_json.get() );
            game_object->menu_items.push_back({
                [ commodity_content ](){ return deserialize_commodity_content( commodity_content.c_str() ); },
                [ game_object , commodity_content ](){ shopping( game_object , commodity_content.c_str() ); }
            });
        }
        game_object->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_object ](){ close_store_menu_v2( game_object ); }
        });
    }

    std::string deserialize_commodity_content( const char * content )
    {
        json_error_t json_error;
        json_t * root = json_loads( content , 0 , &json_error );
        if ( root == nullptr )
        {
            json_decref( root );
            return std::string( "无效按钮" );
        }

        json_t * price_type_node = json_object_get( root , "price_type" );
        if ( !json_is_string( price_type_node ) )
        {
            json_decref( root );
            return std::string( "无效按钮" );
        }
        std::string price_type( json_string_value( price_type_node ) );
        json_t * price_node = json_object_get( root , "price" );
        if ( !json_is_integer( price_node ) )
        {
            json_decref( root );
            return std::string( "无效按钮" );
        }
        json_int_t price_value = json_integer_value( price_node );
        json_t * commodity_type_node = json_object_get( root , "commodity_type" );
        if ( !json_is_string( commodity_type_node ) )
        {
            json_decref( root );
            return std::string( "无效按钮" );
        }
        std::string commodity_type( json_string_value( commodity_type_node ) );
        json_t * commodity_value_node = json_object_get( root , "commodity_value" );
        if ( !json_is_integer( commodity_value_node ) )
        {
            json_decref( root );
            return std::string( "无效按钮" );
        }
        json_int_t commodity_value = json_integer_value( commodity_value_node );

        std::string deserialize_string( "支付 ");
        if ( price_type == std::string( "LEVEL" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "级" );
        }
        else if ( price_type == std::string( "LIFE" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "生命" );
        }
        else if ( price_type == std::string( "ATTACK" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "攻击" );
        }
        else if ( price_type == std::string( "DEFENCE" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "防御" );
        }
        else if ( price_type == std::string( "GOLD" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "金币" );
        }
        else if ( price_type == std::string( "EXPERIENCE" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "经验" );
        }
        else if ( price_type == std::string( "YELLOW_KEY" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "黄钥匙" );
        }
        else if ( price_type == std::string( "BLUE_KEY" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "蓝钥匙" );
        }
        else if ( price_type == std::string( "RED_KEY" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "红钥匙" );
        }
        else if ( price_type == std::string( "ALL_KEY" ) )
        {
            deserialize_string += std::to_string( price_value ) + std::string( "各种钥匙" );
        }
        else
        {
            json_decref( root );
            return std::string( "无效按钮" );
        }

        deserialize_string += "购买";
        if ( commodity_type == std::string( "CHANGE_LEVEL" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "级" );
        }
        else if ( commodity_type == std::string( "CHANGE_LIFE" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "生命" );
        }
        else if ( commodity_type == std::string( "CHANGE_ATTACK" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "攻击" );
        }
        else if ( commodity_type == std::string( "CHANGE_DEFENSE" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "防御" );
        }
        else if ( commodity_type == std::string( "CHANGE_GOLD" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "金币" );
        }
        else if ( commodity_type == std::string( "CHANGE_EXPERIENCE" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "经验" );
        }
        else if ( commodity_type == std::string( "CHANGE_YELLOW_KEY" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "黄钥匙" );
        }
        else if ( commodity_type == std::string( "CHANGE_BLUE_KEY" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "蓝钥匙" );
        }
        else if ( commodity_type == std::string( "CHANGE_RED_KEY" ) )
        {
            deserialize_string += std::to_string( commodity_value ) + std::string( "红钥匙" );
        }
        else
        {
            return std::string( "无效按钮" );
        }

        return deserialize_string;
    }

    static void remove_tips( GameEnvironment * game_object )
    {
        if ( game_object->tips_content.empty() )
            return ;
        game_object->tips_content.pop_front();
    }
}
