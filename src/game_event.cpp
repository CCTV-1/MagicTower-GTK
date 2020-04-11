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
    position_t temp_pos;

    static bool open_door( GameStatus * game_status , position_t position );
    static bool change_floor( GameStatus * game_status , std::uint32_t stair_id );
    static std::int64_t get_combat_damage_of_last  ( const Hero& hero , const Monster& monster );
    static std::int64_t get_combat_damage_of_normal( const Hero& hero , const Monster& monster );
    static std::int64_t get_combat_damage_of_first ( const Hero& hero , const Monster& monster );
    static std::int64_t get_combat_damage_of_double( const Hero& hero , const Monster& monster );
    static bool get_item( GameStatus * game_status , std::uint32_t item_id );
    static void set_grid_type( GameStatus * game_status , position_t position , GRID_TYPE type_id = GRID_TYPE::FLOOR );
    static void set_tips( GameStatus * game_status , std::string tips_content );
    static void set_jump_menu( GameStatus * game_status );
    static void set_start_menu( GameStatus * game_status );
    static void set_game_menu( GameStatus * game_status );
    static void set_inventories_menu( GameStatus * game_status );
    static void set_store_menu( GameStatus * game_status );
    static void set_sub_store_menu( GameStatus * game_status , std::uint32_t store_id );

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
        //path not found return
        if ( came_from.find( current ) == came_from.end() )
            return {};
        while ( current != start )
        {
            path.push_back( current );
            current = came_from[current];
            //path not found return
            if ( came_from.find( current ) == came_from.end() )
                return {};
        }
        return path;
    }

    static inline double heuristic( TowerGridLocation a , TowerGridLocation b )
    {
        return std::abs( a.x - b.x ) + std::abs( a.y - b.y );
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

    bool operator==( const EventPosition& lhs , const EventPosition& rhs )
    {
        return ( ( lhs.floor == rhs.floor ) && ( lhs.x == rhs.x ) && ( lhs.y == rhs.y ) );
    }
    bool operator!=( const EventPosition& lhs , const EventPosition& rhs )
    {
        return !( lhs == rhs );
    }

    void scriptengines_register_eventfunc( GameStatus * game_status )
    {
        const luaL_Reg funcs[] =
        {
            {
                //void set_volume( number )
                "set_volume" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_set_volume" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    double volume = luaL_checknumber( L , 1 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->music.set_volume( volume );
                    return 0;
                }
            },
            {
                //number get_volume( void )
                "get_volume" , []( lua_State * L ) -> int
                {
                    //discard any extra arguments passed
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    double volume = game_status->music.get_volume();
                    lua_pushnumber( L , volume );
                    return 1;
                }
            },
            {
                //void set_playmode( integer )
                "set_playmode" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_set_volume" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    PLAY_MODE mode_value = static_cast<PLAY_MODE>( luaL_checkinteger( L , 1 ) );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->music.set_playmode( mode_value );
                    return 0;
                }
            },
            {
                //void set_playlist( string list uri )
                "set_playlist" , []( lua_State * L ) -> int
                {
                    std::uint32_t top = lua_gettop( L );
                    if ( top < 1 )
                    {
                        g_log( "lua_set_playlist" , G_LOG_LEVEL_WARNING , "expecting exactly 1 or more arguments" );
                        return 0;
                    }
                    std::vector<std::string> new_playlist;
                    for( std::size_t i = 1 ; i <= top ; i++ )
                    {
                        new_playlist.push_back( std::string( luaL_checkstring( L , i ) ) );
                    }
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->music.set_playlist( new_playlist );
                    return 0;
                }
            },
            {
                //void play_next( void )
                "play_next" , []( lua_State * L ) -> int
                {
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->music.next();
                    return 0;
                }
            },
            {
                //void play_pause( void )
                "play_pause" , []( lua_State * L ) -> int
                {
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->music.pause();
                    return 0;
                }
            },
            {
                //void play_resume( void )
                "play_resume" , []( lua_State * L ) -> int
                {
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->music.resume();
                    return 0;
                }
            },
            {
                //void set_tips( string )
                "set_tips" , []( lua_State * L ) -> int
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
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    set_tips( game_status , tips );
                    return 0;
                }
            },
            {
                //void set_grid_type( table position , integer grid_id )
                "set_grid_type" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 2 )
                    {
                        g_log( "lua_set_grid_type" , G_LOG_LEVEL_WARNING , "expecting exactly 2 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 2 );
                    luaL_checktype( L , 1 , LUA_TTABLE );
                    lua_getfield( L , 1 , "floor" );
                    lua_getfield( L , 1 , "x" );
                    lua_getfield( L , 1 , "y" );
                    std::uint32_t type_value = luaL_checkinteger( L , 2 );
                    std::uint32_t floor = luaL_checkinteger( L , 3 );
                    std::uint32_t x = luaL_checkinteger( L , 4 );
                    std::uint32_t y = luaL_checkinteger( L , 5 );
                    GRID_TYPE grid_type = static_cast<GRID_TYPE>( type_value );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    set_grid_type( game_status , { floor , x , y  } , grid_type );
                    return 0;
                }
            },
            {
                //integer get_grid_type( table position )
                "get_grid_type" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_get_grid_type" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    luaL_checktype( L , 1 , LUA_TTABLE );
                    lua_getfield( L , 1 , "floor" );
                    lua_getfield( L , 1 , "x" );
                    lua_getfield( L , 1 , "y" );
                    std::uint32_t floor = luaL_checkinteger( L , 2 );
                    std::uint32_t x = luaL_checkinteger( L , 3 );
                    std::uint32_t y = luaL_checkinteger( L , 4 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    TowerGrid grid = game_status->game_map.get_grid( floor , x , y );
                    lua_pushinteger( L , static_cast<std::uint32_t>( grid.type ) );
                    return 1;
                }
            },
            {
                //void set_grid( table position , table grid )
                "set_grid" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 2 )
                    {
                        g_log( "lua_set_grid" , G_LOG_LEVEL_WARNING , "expecting exactly 2 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 2 );
                    luaL_checktype( L , 1 , LUA_TTABLE );
                    luaL_checktype( L , 2 , LUA_TTABLE );
                    lua_getfield( L , 1 , "floor" );
                    lua_getfield( L , 1 , "x" );
                    lua_getfield( L , 1 , "y" );
                    lua_getfield( L , 2 , "type" );
                    lua_getfield( L , 2 , "id" );
                    std::uint32_t floor = luaL_checkinteger( L , 3 );
                    std::uint32_t x = luaL_checkinteger( L , 4 );
                    std::uint32_t y = luaL_checkinteger( L , 5 );
                    GRID_TYPE grid_type = static_cast<GRID_TYPE>( luaL_checkinteger( L , 6 ) );
                    std::uint32_t grid_id = luaL_checkinteger( L , 7 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->game_map.set_grid( floor , x , y , { grid_type , grid_id } );
                    return 0;
                }
            },
            {
                //table get_grid( table position )
                "get_grid" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_get_grid" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    luaL_checktype( L , 1 , LUA_TTABLE );
                    lua_getfield( L , 1 , "floor" );
                    lua_getfield( L , 1 , "x" );
                    lua_getfield( L , 1 , "y" );
                    std::uint32_t floor = luaL_checkinteger( L , 2 );
                    std::uint32_t x = luaL_checkinteger( L , 3 );
                    std::uint32_t y = luaL_checkinteger( L , 4 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    TowerGrid grid = game_status->game_map.get_grid( floor , x , y );
                    lua_newtable( L );
                    lua_pushinteger( L , static_cast<std::uint32_t>( grid.type ) );
                    lua_setfield( L , 5 , "type" );
                    lua_pushinteger( L , grid.id );
                    lua_setfield( L , 5 , "id" );
                    return 1;
                }
            },
            {
                //table get_hero_property( void )
                "get_hero_property" , []( lua_State * L ) -> int
                {
                    //arguments number impossible less than 0,don't need check
                    //discard any extra arguments passed
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    Hero& hero = game_status->hero;
                    lua_pushhero( L , hero );
                    return 1;
                }
            },
            {
                //void set_hero_property( table hero )
                "set_hero_property" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_set_hero_property" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->hero = lua_gethero( L , 1 );
                    return 0;
                }
            },
            {
                //integer get_flag( string flag_name )
                //if flag not exist,reutn nil
                "get_flag" , []( lua_State * L ) -> int
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
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    if ( game_status->script_flags.find( flags_name ) == game_status->script_flags.end() )
                    {
                        lua_pushnil( L );
                        return 1;
                    }
                    std::int64_t value = game_status->script_flags[flags_name];
                    lua_pushinteger( L , value );
                    return 1;
                }
            },
            {
                //void set_flag( string flag_name , integer flag_value )
                "set_flag" , []( lua_State * L ) -> int
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
                    std::int64_t value = lua_tointeger( L , 2 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->script_flags[flags_name] = value;
                    return 0;
                }
            },
            {
                //void open_dialog( string list dialog_content )
                "open_dialog" , []( lua_State * L ) -> int
                {
                    std::size_t argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_get_item" , G_LOG_LEVEL_WARNING , "expecting exactly 1 or more arguments" );
                        return 0;
                    }
                    std::deque<std::string> messages;
                    for ( std::size_t i = 1 ; i <= argument_count ; i++ )
                    {
                        messages.push_back( luaL_checkstring( L , i ) );
                    }
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->game_message = messages;
                    game_status->state = GAME_STATE::MESSAGE;
                    return 0;
                }
            },
            {
                //void open_menu( table of ( string item_name , string item_func ) )
                "open_menu" , []( lua_State * L ) -> int
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
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->menu_items = {};
                    lua_pushnil( L );
                    while( lua_next( L , 1 ) )
                    {
                        if ( ( lua_type( L , 3 ) == LUA_TFUNCTION ) && ( lua_type( L , 2 ) == LUA_TSTRING ) )
                        {
                            std::string item_name( lua_tostring( L , 2 ) );
                            std::uint32_t refvalue = 0;
                            if ( game_status->refmap.find( item_name ) != game_status->refmap.end() )
                            {
                                refvalue = game_status->refmap[ item_name ];
                                lua_pop( L , 1 );
                            }
                            else
                            {
                                refvalue = luaL_ref( L , LUA_REGISTRYINDEX );
                                game_status->refmap[ item_name ] = refvalue;
                            }
                            game_status->menu_items.push_back({
                                [ item_name ](){ return item_name; },
                                [ L , refvalue ](){
                                    lua_rawgeti( L , LUA_REGISTRYINDEX , refvalue );
                                    lua_call( L , 0 , 0 );
                                }
                            });
                        }
                        else
                        {
                            //luaL_ref pop stack
                            lua_pop( L , 1 );
                        }
                    }
                    game_status->menu_items.push_back({
                        [](){ return std::string( "关闭菜单" ); },
                        [ game_status ](){ game_status->state = GAME_STATE::NORMAL; }
                    });
                    game_status->state = GAME_STATE::GAME_MENU;
                    game_status->focus_item_id = 0;
                    return 0;
                }
            },
            {
                //void close_menu( void )
                "close_menu" , []( lua_State * L ) -> int
                {
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    if ( game_status->state == GAME_STATE::GAME_MENU )
                    {
                        game_status->state = GAME_STATE::NORMAL;
                    }
                    return 0;
                }
            },
            {
                //void get_item( integer item_id )
                "get_item" , []( lua_State * L ) -> int
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
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    get_item( game_status , item_id );
                    return 0;
                }
            },
            {
                //void unlock_store( integer store_id )
                "unlock_store" , []( lua_State * L ) -> int
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
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->stores[ store_id ].usability = true;
                    std::string flag_name = std::string( "store_" ) + std::to_string( store_id );
                    game_status->script_flags[ flag_name ] = 1;
                    std::string tips = std::string( "解锁商店:" ) + ( game_status->stores[ store_id ] ).store_name;
                    set_tips( game_status , tips );
                    return 0;
                }
            },
            {
                //void lock_store( integer store_id )
                "lock_store" , []( lua_State * L ) -> int
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
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_status->stores[ store_id ].usability = false;
                    std::string flag_name = std::string( "store_" ) + std::to_string( store_id );
                    game_status->script_flags.erase( flag_name );
                    std::string tips = std::string( "锁定商店:" ) + ( game_status->stores[ store_id ] ).store_name;
                    set_tips( game_status , tips );
                    return 0;
                }
            },
            {
                //void move_hero( table position )
                "move_hero" , []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_move_hero" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    luaL_checktype( L , 1 , LUA_TTABLE );
                    lua_getfield( L , 1 , "floor" );
                    lua_getfield( L , 1 , "x" );
                    lua_getfield( L , 1 , "y" );
                    std::uint32_t floor = luaL_checkinteger( L , 2 );
                    std::uint32_t x = luaL_checkinteger( L , 3 );
                    std::uint32_t y = luaL_checkinteger( L , 4 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    if ( floor > game_status->game_map.map.size() )
                    {
                        set_tips( game_status , std::string( "floor id: '" ) + std::to_string( floor ) + std::string( "' illegal" ) );
                        return 0;
                    }
                    if ( game_status->state == GAME_STATE::FIND_PATH )
                    {
                        game_status->state = GAME_STATE::NORMAL;
                    }
                    Hero& hero = game_status->hero;
                    hero.x = x;
                    hero.y = y;
                    hero.floors = floor;
                    return 0;
                }
            },
            {
                //void game_win( void )
                "game_win" , []( lua_State * L ) -> int
                {
                    //arguments number impossible less than 0,don't need check
                    //discard any extra arguments passed
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_win( game_status );
                    return 0;
                }
            },
            {
                //void game_lose( void )
                "game_lose" , []( lua_State * L ) -> int
                {
                    //arguments number impossible less than 0,don't need check
                    //discard any extra arguments passed
                    lua_settop( L , 0 );
                    GameStatus * game_status = ( GameStatus * )( lua_topointer( L , lua_upvalueindex( 1 ) ) );
                    game_lose( game_status );
                    return 0;
                }
            }
        };
        lua_State * L = game_status->script_engines.get();
        int top = lua_gettop( L );
        lua_getglobal( L , "_G" );
        for ( std::size_t i = 0 ; i < sizeof( funcs )/sizeof( luaL_Reg ) ; i++ )
        {
            lua_pushstring( L , funcs[i].name );
            lua_pushlightuserdata( L , ( void * )game_status );
            lua_pushcclosure( L , funcs[i].func , 1 );
            lua_rawset( L , top + 1 );
        }
        lua_pop( L , 1 );
    }

    std::vector<TowerGridLocation> find_path( GameStatus * game_status , TowerGridLocation goal , std::function<bool(std::uint32_t x , std::uint32_t y)> is_visible )
    {
        Hero& hero = game_status->hero;
        TowerMap& tower = game_status->game_map;
        if ( ( is_visible != nullptr ) && ( !is_visible( goal.x , goal.y ) ) )
        {
            return {};
        }
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
                if ( ( is_visible != nullptr ) && ( !is_visible( x , y ) ) )
                {
                    game_map.walls.insert( { std::int64_t( x ) , std::int64_t( y ) } );
                    continue;
                }
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

    void save_game( GameStatus * game_status , size_t save_id )
    {
        Glib::RefPtr<Gio::File> save_dir = Gio::File::create_for_path( ResourcesManager::get_save_path() );
        try
        {
            save_dir->make_directory_with_parents();
        }
        catch( const Glib::Error& e )
        {
            //if dir exists,do nothing.
            g_log( __func__ , G_LOG_LEVEL_WARNING , "%s" , e.what().c_str() );
        }

        std::string fail_tips = std::string( "保存存档:" ) + std::to_string( save_id ) + std::string( "失败" );
        try
        {
            DataBase db( ResourcesManager::get_save_path() + std::to_string( save_id ) + std::string( ".db" ) );
            db.set_tower_info( game_status->game_map );
            db.set_hero_info( game_status->hero , 0 );
            db.set_script_flags( game_status->script_flags );
            db.set_inventories( game_status->inventories );
        }
        catch ( const std::runtime_error& e )
        {
            set_tips( game_status , fail_tips );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s" , e.what() );
            return ;
        }
        std::string tips = std::string( "保存存档:" ) + std::to_string( save_id ) + std::string( "成功" );
        set_tips( game_status , tips );
    }

    void load_game( GameStatus * game_status , size_t save_id )
    {
        std::string fail_tips = std::string( "读取存档:" ) + std::to_string( save_id ) + std::string( "失败" );
        Glib::RefPtr<Gio::File> db_file = Gio::File::create_for_path( ResourcesManager::get_save_path() + std::to_string( save_id ) + std::string( ".db" ) );
        if ( db_file->query_exists() == false )
        {
            fail_tips = std::string( "存档:" ) + std::to_string( save_id ) + std::string( "不存在" );
            set_tips( game_status , fail_tips );
            return ;
        }
        try
        {
            DataBase db( ResourcesManager::get_save_path() + std::to_string( save_id ) + std::string( ".db" ) );
            game_status->game_map = db.get_tower_info();
            game_status->hero = db.get_hero_info( 0 );
            game_status->script_flags = db.get_script_flags();
            game_status->inventories = db.get_inventories();

            //store unlock flag
            for ( auto& store : game_status->stores )
            {
                std::string flag_name = std::string( "store_" ) + std::to_string( store.first );
                if ( game_status->script_flags.find( flag_name ) != game_status->script_flags.end() )
                {
                    store.second.usability = true;
                }
                else
                {
                    store.second.usability = false;
                }
                
            }

            //floor jump unlock flag
            for ( std::uint32_t i = 0 ; i < game_status->game_map.map.size() ; i++ )
            {
                std::string floor_flag = std::string( "floors_" ) + std::to_string( i );
                if ( game_status->script_flags.find( floor_flag ) != game_status->script_flags.end() )
                {
                    game_status->access_floor[i] = true;
                }
                else
                {
                    game_status->access_floor[i] = false;
                }
            }
        }
        catch ( const std::runtime_error& e )
        {
            set_tips( game_status , fail_tips );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s" , e.what() );
            return ;
        }
        std::string tips = std::string( "读取存档:" ) + std::to_string( save_id ) + std::string( "成功" );
        set_tips( game_status , tips );
    }

    std::int64_t get_combat_damage( GameStatus * game_status , std::uint32_t monster_id )
    {
        if ( game_status->monsters.find( monster_id ) == game_status->monsters.end() )
        {
            return -1;
        }
        Hero& hero = game_status->hero;
        Monster& monster = game_status->monsters[ monster_id ];
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

    bool battle( GameStatus * game_status , std::uint32_t monster_id )
    {
        if ( game_status->monsters.find( monster_id ) == game_status->monsters.end() )
            return false;
        Hero& hero = game_status->hero;
        Monster& monster = game_status->monsters[ monster_id ];
        std::int64_t damage = get_combat_damage( game_status , monster_id );
        if ( ( damage < 0 ) || ( damage >= hero.life ) )
            return false;
        game_status->soundeffect_player.play( ResourcesManager::get_soundeffect_uri( "attack" ) );
        hero.life =  hero.life - damage;
        hero.gold = hero.gold + monster.gold;
        hero.experience = hero.experience + monster.experience;
        std::string tips = std::string( "击杀:" ) + monster.name + std::string( ",获取金币:" ) + std::to_string( monster.gold )
            + std::string( ",经验:" ) + std::to_string( monster.experience );
        set_tips( game_status , tips );
        return true;
    }

    bool move_hero( GameStatus * game_status , const position_t& new_pos )
    {
        Hero& hero = game_status->hero;
        TowerMap& tower = game_status->game_map;
        if ( tower.map.find( new_pos.floor ) == tower.map.end() )
            return false;
        if ( new_pos.x >= tower.map[new_pos.floor].length )
            return false;
        if ( new_pos.y >= tower.map[new_pos.floor].width )
            return false;
        position_t old_pos = { hero.floors , hero.x , hero.y };
        if ( old_pos == new_pos )
            return true;

        hero.floors = new_pos.floor;
        hero.x = new_pos.x;
        hero.y = new_pos.y;

        Glib::ustring script_name = Glib::ustring::compose( "%1F%2_%3_%4.lua" , ResourcesManager::get_script_path() , game_status->hero.floors ,
            game_status->hero.x , game_status->hero.y );
        int res = luaL_loadfilex( game_status->script_engines.get() , script_name.data() , "t" );
        if ( res == LUA_OK )
        {
            if ( lua_pcall( game_status->script_engines.get() , 0 , 0 , 0 ) != LUA_OK )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "execute script:'%s' failure,error message:'%s'" , script_name.data() , lua_tostring( game_status->script_engines.get() , -1 ) );
                lua_pop( game_status->script_engines.get() , 1 );
            }
        }
        else
        {
            //if undefined this position script,ignore
            if ( res != LUA_ERRFILE )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "loading script:'%s' failure,error message:'%s'" , script_name.data() , lua_tostring( game_status->script_engines.get() , -1 ) );
            }
            lua_pop( game_status->script_engines.get() , 1 );
        }
        if ( new_pos != position_t({ hero.floors , hero.x , hero.y }) )
        {
            return false;
        }

        bool state = false;
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
                state = change_floor( game_status , grid.id );
                break;
            }
            case GRID_TYPE::DOOR:
            {
                open_door( game_status , { hero.floors , hero.x , hero.y } );
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
                state = battle( game_status , grid.id );
                if ( state )
                    set_grid_type( game_status , { hero.floors , hero.x , hero.y } );
                else
                {
                    game_lose( game_status );
                }
                state = false;
                break;
            }
            case GRID_TYPE::ITEM:
            {
                state = get_item( game_status , grid.id );
                if ( state )
                    set_grid_type( game_status , { hero.floors , hero.x , hero.y } );
                else
                {
                    set_tips( game_status , "你太弱了 拿不起来" );
                }
                break;
            }
            default :
            {
                state = false;
                break;
            }
        }

        script_name = Glib::ustring::compose( "%1L%2_%3_%4.lua" , ResourcesManager::get_script_path() , game_status->hero.floors ,
            game_status->hero.x , game_status->hero.y );
        res = luaL_loadfilex( game_status->script_engines.get() , script_name.data() , "t" );
        if ( res == LUA_OK )
        {
            if ( lua_pcall( game_status->script_engines.get() , 0 , 0 , 0 ) != LUA_OK )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "execute script:'%s' failure,error message:'%s'" , script_name.data() , lua_tostring( game_status->script_engines.get() , -1 ) );
                lua_pop( game_status->script_engines.get() , 1 );
            }
        }
        else
        {
            //if undefined this position script,ignore
            if ( res != LUA_ERRFILE )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "loading script:'%s' failure,error message:'%s'" , script_name.data() , lua_tostring( game_status->script_engines.get() , -1 ) );
            }
            lua_pop( game_status->script_engines.get() , 1 );
        }
        if ( !state )
        {
            hero.floors = old_pos.floor;
            hero.x = old_pos.x;
            hero.y = old_pos.y;
            return false;
        }

        return true;
    }

    bool use_item( GameStatus * game_status , std::uint32_t item_id )
    {
        if ( game_status->items.find( item_id ) == game_status->items.end() )
            return false;
        Item& item = game_status->items[ item_id ];

        if ( game_status->refmap.find( item.item_detail ) == game_status->refmap.end() )
        {
            return false;
        }
        std::uint32_t refvalue = game_status->refmap[item.item_detail];
        if ( refvalue == 0 )
        {
            return false;
        }
        lua_rawgeti( game_status->script_engines.get() , LUA_REGISTRYINDEX , refvalue );
        
        if ( lua_pcall( game_status->script_engines.get() , 0 , 0 , 0 ) != LUA_OK )
        {
            set_tips( game_status , lua_tostring( game_status->script_engines.get() , -1 ) );
            lua_pop( game_status->script_engines.get() , 1 );
            return false;
        }
        return true;
    }

    void open_floor_jump( GameStatus * game_status )
    {
        switch ( game_status->state )
        {
            case GAME_STATE::GAME_LOSE:
            case GAME_STATE::GAME_WIN:
            case GAME_STATE::GAME_MENU:
            case GAME_STATE::STORE_MENU:
            case GAME_STATE::JUMP_MENU:
                return ;
            default:
                break;
        }

        if ( game_status->game_map.map.find( game_status->hero.floors ) == game_status->game_map.map.end() )
        {
            set_tips( game_status , "这层楼禁止使用楼层跳跃器！" );
            return ;
        }

        game_status->state = GAME_STATE::JUMP_MENU;
        game_status->focus_item_id = 0;
        //save initial position
        temp_pos= { game_status->hero.floors , game_status->hero.x , game_status->hero.y };
        //hidden hero
        game_status->hero.x = game_status->game_map.map[ game_status->hero.floors ].length;
        game_status->hero.y = game_status->game_map.map[ game_status->hero.floors ].width;
        set_jump_menu( game_status );
    }

    void close_floor_jump( GameStatus * game_status )
    {
        if ( game_status->state != GAME_STATE::JUMP_MENU )
            return ;
        game_status->hero.floors = temp_pos.floor;
        game_status->hero.x = temp_pos.x;
        game_status->hero.y = temp_pos.y;
        game_status->state = GAME_STATE::NORMAL;
    }

    void open_store_menu( GameStatus * game_status )
    {
        switch ( game_status->state )
        {
            case GAME_STATE::GAME_LOSE:
            case GAME_STATE::GAME_WIN:
            case GAME_STATE::GAME_MENU:
            case GAME_STATE::STORE_MENU:
            case GAME_STATE::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_status->state = GAME_STATE::STORE_MENU;
        game_status->focus_item_id = 0;
        set_store_menu( game_status );
    }

    void close_store_menu( GameStatus * game_status )
    {
        if ( game_status->state != GAME_STATE::STORE_MENU )
            return ;
        game_status->state = GAME_STATE::NORMAL;
    }

    void open_start_menu( GameStatus * game_status )
    {
        switch ( game_status->state )
        {
            case GAME_STATE::GAME_LOSE:
            case GAME_STATE::GAME_WIN:
                break;
            default:
                return ;
        }

        game_status->state = GAME_STATE::START_MENU;
        game_status->focus_item_id = 0;
        set_start_menu( game_status );
    }

    void open_game_menu( GameStatus * game_status )
    {
        switch ( game_status->state )
        {
            case GAME_STATE::GAME_LOSE:
            case GAME_STATE::GAME_WIN:
            case GAME_STATE::GAME_MENU:
            case GAME_STATE::STORE_MENU:
            case GAME_STATE::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_status->state = GAME_STATE::GAME_MENU;
        game_status->focus_item_id = 0;
        set_game_menu( game_status );
    }

    void close_game_menu( GameStatus * game_status )
    {
        if ( game_status->state != GAME_STATE::GAME_MENU )
            return ;
        game_status->state = GAME_STATE::NORMAL;
    }

    void open_inventories_menu( GameStatus * game_status )
    {
        game_status->state = GAME_STATE::INVENTORIES_MENU;
        game_status->focus_item_id = 0;
        set_inventories_menu( game_status );
    }

    void close_inventories_menu( GameStatus * game_status )
    {
        if ( game_status->state != GAME_STATE::INVENTORIES_MENU )
            return ;
        game_status->state = GAME_STATE::NORMAL;
    }

    void game_win( GameStatus * game_status )
    {
        switch ( game_status->state )
        {
            case GAME_STATE::GAME_LOSE:
            case GAME_STATE::GAME_WIN:
            case GAME_STATE::GAME_MENU:
            case GAME_STATE::STORE_MENU:
            case GAME_STATE::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_status->game_message.push_back( "恭喜通关" );
        game_status->game_message.push_back(
            std::string( "你的得分为:" ) + std::to_string(
                game_status->hero.life + ( game_status->hero.attack +
                game_status->hero.defense )*10 + ( game_status->hero.level )*100
            )
        );
        game_status->state = GAME_STATE::GAME_WIN;
    }

    void game_lose( GameStatus * game_status )
    {
        switch ( game_status->state )
        {
            case GAME_STATE::GAME_LOSE:
            case GAME_STATE::GAME_WIN:
            case GAME_STATE::GAME_MENU:
            case GAME_STATE::STORE_MENU:
            case GAME_STATE::JUMP_MENU:
                return ;
            default:
                break;
        }

        game_status->game_message.push_back( "游戏失败" );
        game_status->game_message.push_back(
            std::string( "你的得分为:" ) + std::to_string(
                game_status->hero.life + ( game_status->hero.attack +
                game_status->hero.defense )*( game_status->hero.level )
            )
        );
        game_status->state = GAME_STATE::GAME_LOSE;
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

    static void set_grid_type( GameStatus * game_status , position_t position , GRID_TYPE type_id )
    {
        if ( game_status->game_map.map.find( position.floor ) == game_status->game_map.map.end() )
        {
            //do nothing
            return ;
        }
        //TowerMap::set_grid check x , y overflow,no need to check here
        std::uint32_t defaultid = game_status->game_map.map[ position.floor ].default_floorid;
        game_status->game_map.set_grid( position.floor , position.x , position.y , { type_id , defaultid } );
    }

    static void set_tips( GameStatus * game_status , std::string tips_content )
    {
        game_status->tips_content.push_back( tips_content );
        Glib::signal_timeout().connect_once( 
            [ game_status ]()
            {
                if ( game_status->tips_content.empty() )
                    return ;
                game_status->tips_content.pop_front();
            }
        , 1000 );
    }

    static bool open_door( GameStatus * game_status , position_t position )
    {
        Hero& hero = game_status->hero;
        TowerMap& tower = game_status->game_map;
        auto grid = tower.get_grid( position.floor , position.x , position.y );
        if ( grid.type != GRID_TYPE::DOOR )
            return false;

        switch( grid.id )
        {
            case 1:
            {
                if ( hero.yellow_key >= 1 )
                {
                    set_grid_type( game_status , position );
                    hero.yellow_key--;
                    game_status->soundeffect_player.play( ResourcesManager::get_soundeffect_uri( "door" ) );
                }
                break;
            }
            case 2:
            {
                if ( hero.blue_key >= 1 )
                {
                    set_grid_type( game_status , position );
                    hero.blue_key--;
                    game_status->soundeffect_player.play( ResourcesManager::get_soundeffect_uri( "door" ) );
                }
                break;
            }
            case 3:
            {
                if ( hero.red_key >= 1 )
                {
                    set_grid_type( game_status , position );
                    hero.red_key--;
                    game_status->soundeffect_player.play( ResourcesManager::get_soundeffect_uri( "door" ) );
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

    static bool change_floor( GameStatus * game_status , std::uint32_t stair_id )
    {
        if ( game_status->stairs.find( stair_id ) == game_status->stairs.end() )
            return false;
        Stairs stair = game_status->stairs[ stair_id ];
        TowerMap& tower = game_status->game_map;

        if ( tower.map.find( stair.floors ) == tower.map.end() )
            return false;
        if ( stair.x >= tower.map[stair.floors].length )
            return false;
        if ( stair.y >= tower.map[stair.floors].width )
            return false;

        game_status->soundeffect_player.play( ResourcesManager::get_soundeffect_uri( "floor" ) );
        game_status->hero.floors = stair.floors;
        game_status->hero.x = stair.x;
        game_status->hero.y = stair.y;

        game_status->access_floor[ stair.floors ] = true;
        std::string floor_flag = std::string( "floors_" ) + std::to_string( stair.floors );
        game_status->script_flags[floor_flag] = 1;

        return true;
    }

    static bool get_item( GameStatus * game_status , std::uint32_t item_id )
    {
        if ( game_status->items.find( item_id ) == game_status->items.end() )
            return false;
        Item& item = game_status->items[ item_id ];

        if ( !item.needactive )
        {
            if ( !use_item( game_status , item_id ) )
            {
                return false;
            }
        }
        else
        {
            game_status->inventories[item_id]++;
        }

        game_status->soundeffect_player.play( ResourcesManager::get_soundeffect_uri( "item" ) );
        std::string tips = std::string( "获得:'" ) + item.item_name + std::string( "'" );
        set_tips( game_status , tips );
        return true;
    }

    static void set_jump_menu( GameStatus * game_status )
    {
        game_status->menu_items.clear();
        game_status->menu_items.push_back({
            [](){ return std::string( "最上层" ); },
            [ game_status ](){
                auto end_iter = game_status->game_map.map.end();
                game_status->hero.floors = ( std::prev( end_iter ) )->first;
            }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "上一层" ); },
            [ game_status ](){
                auto current_iter = game_status->game_map.map.find( game_status->hero.floors );
                if ( ( current_iter = std::next( current_iter ) ) != game_status->game_map.map.end() )
                {
                    game_status->hero.floors = current_iter->first;
                }
                else
                {
                    std::string tips = std::string( "已是最上层" );
                }
            }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "下一层" ); },
            [ game_status ](){
                auto current_iter = game_status->game_map.map.find( game_status->hero.floors );
                if ( current_iter != game_status->game_map.map.begin() )
                {
                    current_iter = std::prev( current_iter );
                    game_status->hero.floors = current_iter->first;
                }
                else
                {
                    std::string tips = std::string( "已是最下层" );
                    set_tips( game_status , tips );
                }
            }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "最下层" ); },
            [ game_status ](){
                auto begin_iter = game_status->game_map.map.begin();
                game_status->hero.floors = begin_iter->first;
            }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "确定跳跃" ); },
            [ game_status ](){
                //floor change menu have done the check
                //don't need check game_status->game_map.map.find( game_status->hero.floors ) != game_status->game_map.map.end();
                auto& floor_ref = game_status->game_map.map[ game_status->hero.floors ];
                if ( !floor_ref.teleport_point.has_value() )
                {
                    set_tips( game_status , std::string( "所选择的楼层禁止跃入" ) );
                    return ;
                }
                if ( !game_status->access_floor[ game_status->hero.floors ] )
                {
                    set_tips( game_status , std::string( "所选择的楼层当前禁止跃入" ) );
                    return ;
                }
                std::uint32_t floor = game_status->hero.floors;
                std::uint32_t x = floor_ref.teleport_point.value().x;
                std::uint32_t y = floor_ref.teleport_point.value().y;
                //close_floor_jump rollback hero position
                close_floor_jump( game_status );
                game_status->hero.floors = floor;
                game_status->hero.x = x;
                game_status->hero.y = y;
            }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "取消跳跃" ); },
            [ game_status ](){
                close_floor_jump( game_status );
            }
        });
    }

    static void set_start_menu( GameStatus * game_status )
    {
        game_status->menu_items.clear();
        game_status->menu_items.push_back({
            [](){ return std::string( "重新游戏" ); },
            [ game_status ](){
                game_status->initial_gamedata();
            }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "读取存档" ); },
            [ game_status ](){
                load_game( game_status , 1 );
                game_status->state = GAME_STATE::NORMAL;
            }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "退出游戏" ); },
            [ game_status ](){
                game_status->state = GAME_STATE::GAME_END;
            }
        });
    }

    static void set_game_menu( GameStatus * game_status )
    {
        game_status->menu_items.clear();

        game_status->menu_items.push_back({
            [](){ return std::string( "保存存档" ); },
            [ game_status ](){ save_game( game_status , 1 ); }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "读取存档" ); },
            [ game_status ](){ load_game( game_status , 1 ); }
        });
        game_status->menu_items.push_back({
            [ game_status ](){
                if ( game_status->music.get_state() == PLAY_STATE::PAUSE )
                    return std::string( "背景音乐: 关" );
                else
                    return std::string( "背景音乐: 开" );
            },
            [ game_status ](){
                if ( game_status->music.get_state() == PLAY_STATE::PAUSE )
                    game_status->music.resume();
                else
                    game_status->music.pause();
            }
        });
        game_status->menu_items.push_back({
            [ game_status ](){
                if ( game_status->soundeffect_player.get_state() == PLAY_STATE::PAUSE )
                    return std::string( "游戏音效: 关" );
                else
                    return std::string( "游戏音效: 开" );
            },
            [ game_status ](){
                if ( game_status->soundeffect_player.get_state() == PLAY_STATE::PAUSE )
                    game_status->soundeffect_player.resume();
                else
                    game_status->soundeffect_player.pause();
            }
        });
        game_status->menu_items.push_back({
            [ game_status ](){
                if ( game_status->draw_path )
                    return std::string( "寻路指示: 开" );
                else
                    return std::string( "寻路指示: 关" );
            },
            [ game_status ](){ game_status->draw_path = !game_status->draw_path; }
        });
        game_status->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_status ](){ close_game_menu( game_status ); }
        });
    }

    static void set_inventories_menu( GameStatus * game_status )
    {
        game_status->menu_items.clear();
        for ( auto& item : game_status->inventories )
        {
            std::uint32_t item_id = item.first;
            std::uint32_t & item_number = item.second;
            if ( item_number == 0 )
            {
                continue;
            }
            std::string item_name = game_status->items[item.first].item_name;
            game_status->menu_items.push_back({
                [ item_name , &item_number ](){ return item_name + std::string( " X " ) + std::to_string( item_number ); },
                [ game_status , item_id ](){
                    if ( game_status->inventories[item_id] == 0 )
                        return ;
                    if ( use_item( game_status , item_id ) )
                    {
                        game_status->inventories[item_id]--;
                    }
                }
            });
        }
        game_status->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_status ](){ close_inventories_menu( game_status ); }
        });
    }

    static void set_store_menu( GameStatus * game_status )
    {
        game_status->menu_items.clear();
        for ( auto& store : game_status->stores )
        {
            if ( !store.second.usability )
            {
                continue;
            }

            std::string store_name = store.second.store_name;
            std::uint32_t store_id = store.first;
            game_status->menu_items.push_back({
                [ store_name ](){ return store_name; },
                [ game_status , store_id ](){ set_sub_store_menu( game_status , store_id ); }
            });
        }
        game_status->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_status ](){ close_store_menu( game_status ); }
        });
    }

    static void set_sub_store_menu( GameStatus * game_status , std::uint32_t store_id )
    {
        game_status->focus_item_id = 0;

        game_status->menu_items.clear();
        game_status->menu_items.push_back({
            [](){ return std::string( "返回上级菜单" ); },
            [ game_status ](){ set_store_menu( game_status ); }
        });
        Store& store = game_status->stores[store_id];
        for ( auto& commoditie : store.commodities )
        {
            game_status->menu_items.push_back({
                [ commoditie ](){ return commoditie; },
                [ game_status , commoditie ](){
                    if ( game_status->refmap.find( commoditie ) == game_status->refmap.end() )
                    {
                        return ;
                    }
                    std::uint32_t refvalue = game_status->refmap[commoditie];
                    if ( refvalue == 0 )
                    {
                        return ;
                    }
                    lua_rawgeti( game_status->script_engines.get() , LUA_REGISTRYINDEX , refvalue );
                    lua_call( game_status->script_engines.get() , 0 , 0 );
                }
            });
        }

        game_status->menu_items.push_back({
            [](){ return std::string( "关闭菜单" ); },
            [ game_status ](){ close_store_menu( game_status ); }
        });
    }
}
