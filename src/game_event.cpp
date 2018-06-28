#include <cstdio>
#include <cstdint>
#include <cinttypes>

#include <map>
#include <set>
#include <array>
#include <vector>
#include <utility>
#include <queue>
#include <tuple>
#include <algorithm>

#include <glib.h>

#include "game_event.h"

namespace MagicTower
{
    int TOWER_GRID_SIZE;

    static std::int64_t get_combat_damage_of_last  ( Hero& hero , Monster& monster );
    static std::int64_t get_combat_damage_of_normal( Hero& hero , Monster& monster );
    static std::int64_t get_combat_damage_of_first ( Hero& hero , Monster& monster );
    static std::int64_t get_combat_damage_of_double( Hero& hero , Monster& monster );
    static gboolean do_shopping( GtkWidget * widget , gpointer data );
    static GtkWidget * make_commodity_grid( struct GameEnvironment * game_object , std::string content );
    static std::string deserialize_commodity_content( const char * );

    // Helpers for TowerGridLocation
    bool operator==( TowerGridLocation a , TowerGridLocation b )
    {
    	return a.x == b.x && a.y == b.y;
    }

    bool operator!=( TowerGridLocation a , TowerGridLocation b )
    {
    	return !( a == b );
    }

    bool operator<( TowerGridLocation a, TowerGridLocation b )
    {
    	return std::tie( a.x , a.y ) < std::tie( b.x , b.y );
    }

    struct SquareGrid
    {
    	static std::array<TowerGridLocation, 4> DIRS;

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

    std::array<TowerGridLocation, 4> SquareGrid::DIRS =
    {
        TowerGridLocation{ 1 , 0  } , TowerGridLocation{ 0 , -1 } ,
        TowerGridLocation{ -1 , 0 } , TowerGridLocation{ 0 , 1 }
    };

    struct GridWithWeights : SquareGrid
    {
    	std::set<TowerGridLocation> forests;
    	GridWithWeights( std::int64_t w, std::int64_t h ) : SquareGrid( w , h ) {}
    	double cost( TowerGridLocation from_node , TowerGridLocation to_node ) const
    	{
            ( void )from_node;
            //efficiency:1;bypass any forests:this->width*this->height;so 
    		return forests.find( to_node ) != forests.end() ? this->width + this->height : 1;
    	}
    };

    template <typename Location>
    std::vector<Location> reconstruct_path( Location start , Location goal ,
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

    inline double heuristic( TowerGridLocation a , TowerGridLocation b )
    {
    	return std::abs( a.x -  b.x ) + std::abs( a.y -  b.y );
    }

    template <typename Location, typename Graph>
    void a_star_search( Graph graph , Location start , Location goal,
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

    std::vector<TowerGridLocation> find_path( struct GameEnvironment * game_object , TowerGridLocation goal )
    {
        Hero& hero = game_object->hero;
        Tower& tower = game_object->towers;
        TowerGridLocation start = { hero.x , hero.y };
        if ( start == goal )
            return {};
        auto goal_grid = get_tower_grid( tower , hero.layers , goal.x , goal.y );
        if ( goal_grid.type == MagicTower::GRID_TYPE::IS_BOUNDARY )
        {
            return {};
        }
        if ( goal_grid.type == MagicTower::GRID_TYPE::IS_WALL )
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
	    			case MagicTower::GRID_TYPE::IS_BOUNDARY:
	    			case MagicTower::GRID_TYPE::IS_WALL:
	    			//case MagicTower::GRID_TYPE::IS_NPC:
	    				game_map.walls.insert( { std::int64_t( x ) , std::int64_t( y ) } );
	    				break;
	    			case MagicTower::GRID_TYPE::IS_FLOOR:
	    			case MagicTower::GRID_TYPE::IS_ITEM:
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

    bool open_door( struct GameEnvironment * game_object , event_position_t position )
    {
        Hero& hero = game_object->hero;
        Tower& tower = game_object->towers;
        auto grid = get_tower_grid( tower , std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
        if ( grid.type != GRID_TYPE::IS_DOOR )
            return false;
        
        bool status = true;
        switch( grid.id )
        {
            case 1:
                ( hero.yellow_key >= 1 ) ? hero.yellow_key-- : status = false;
                break;
            case 2:
                ( hero.blue_key >= 1 ) ? hero.blue_key-- : status = false;
                break;
            case 3:
                ( hero.red_key >= 1 ) ? hero.red_key-- : status = false;
                break;
            default :
                status = false;
                break;
        }

        return status;
    }

    bool check_grid_type( struct GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id )
    {
        auto grid = get_tower_grid( game_object->towers , std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
        return ( grid.id == type_id );
    }

    void set_grid_type( struct GameEnvironment * game_object , event_position_t position , GRID_TYPE type_id )
    {
        TowerGrid target_grid = { type_id , static_cast<std::uint32_t>( 1 ) };
        set_tower_grid( game_object->towers , target_grid , std::get<2>( position ) , std::get<0>( position ) , std::get<1>( position ) );
    }

    bool move_hero( struct GameEnvironment * game_object , event_position_t position )
    {
        Hero& hero = game_object->hero;
        hero.x = std::get<0>( position );
        hero.y = std::get<1>( position );
        hero.layers = std::get<2>( position );
        return true;
    }

    std::int64_t get_combat_damage( struct GameEnvironment * game_object , std::uint32_t monster_id )
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

    bool battle( struct GameEnvironment * game_object , std::uint32_t monster_id )
    {
        if ( monster_id - 1 >= game_object->monsters.size() )
            return false;
        Hero& hero = game_object->hero;
        Monster& monster = game_object->monsters[ monster_id - 1 ];
        std::int64_t damage = get_combat_damage( game_object , monster_id );
        if ( damage < 0 )
        {
            //cant_combat_messge();
            return false;
        }
        hero.life =  hero.life - damage ;
        hero.gold = hero.gold + monster.gold;
        hero.experience = hero.experience + monster.experience;

        return true;
    }

    bool get_item( struct GameEnvironment * game_object , std::uint32_t commodity_id )
    {
        Hero& hero = game_object->hero;
        if ( commodity_id - 1 >= game_object->items.size() )
            return false;
        Item& item = game_object->items[ commodity_id - 1 ];
        switch( item.ability_type )
        {
            case ITEM_TYPE::CHANGE_LEVEL:
            {
                hero.level += item.ability_value;
                hero.life += 1000*item.ability_value;
                hero.attack += 7*item.ability_value;
                hero.defense += 7*item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_LIFE:
            {
                hero.life += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_ATTACK:
            {
                hero.attack += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_DEFENSE:
            {
                hero.defense += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_GOLD:
            {
                hero.gold += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_EXPERIENCE:
            {
                hero.experience += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_YELLOW_KEY:
            {
                hero.yellow_key += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_BLUE_KEY:
            {
                hero.blue_key += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_RED_KEY:
            {
                hero.red_key += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_ALL_KEY:
            {
                hero.yellow_key += item.ability_value;
                hero.blue_key += item.ability_value;
                hero.red_key += item.ability_value;
                break;
            }
            case ITEM_TYPE::CHANGE_ALL_ABILITY:
            {
                hero.life += hero.life/item.ability_value;
                hero.attack += hero.attack/item.ability_value;
                hero.defense += hero.defense/item.ability_value;
                break;
            }
            default :
            {
                break;
            }
        }
        return true;
    }

    bool change_floor( struct GameEnvironment * game_object , std::uint32_t stair_id )
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

        return true;
    }

    bool trigger_collision_event( struct GameEnvironment * game_object )
    {
        bool status = false;
        Hero& hero = game_object->hero;
        Tower& tower = game_object->towers;

        auto grid = get_tower_grid( tower , hero.layers , hero.x , hero.y );
        switch( grid.type )
        {
            case GRID_TYPE::IS_FLOOR:
            {
                status = true;
                break;
            }
            case GRID_TYPE::IS_STAIRS:
            {
                status = change_floor( game_object , grid.id );
                break;
            }
            case GRID_TYPE::IS_DOOR:
            {
                status = open_door( game_object , { hero.x , hero.y , hero.layers } );
                if ( status == true )
                    set_grid_type( game_object , { hero.x , hero.y , hero.layers } );
                status = false;
                break;
            }
            case GRID_TYPE::IS_NPC:
            {
                status = false;
                break;
            }
            case GRID_TYPE::IS_MONSTER:
            {
                status = battle( game_object , grid.id );
                if ( status == true )
                    set_grid_type( game_object , { hero.x , hero.y , hero.layers } );
                else
                {
                    game_lose( game_object );
                }
                
                status = false;
                break;
            }
            case GRID_TYPE::IS_ITEM:
            {
                status = get_item( game_object , grid.id );
                if ( status == true )
                    set_grid_type( game_object , { hero.x , hero.y , hero.layers } );
                break;
            }
            default :
            {
                status = false;
                break;
            }
        }

        auto event_iter = game_object->custom_events.find( { hero.x , hero.y , hero.layers } );
        if ( event_iter != game_object->custom_events.end() )
            trigger_custom_event( game_object , event_iter->second );

        return status;
    }

    bool trigger_custom_event( struct GameEnvironment * game_object , std::shared_ptr<char>& event_json )
    {
        json_error_t json_error;
        json_t * root = json_loads( event_json.get() , 0 , &json_error );
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger event content:\n%s" , event_json.get() );
        json_t * type_node = json_object_get( root , "event_type" );
        if( !json_is_string( type_node ) )
        {
            json_decref( root );
            return false;
        }
        std::string event_type_string( json_string_value( type_node ) );
        //uint32_t uint32_t uint32_t
        if ( event_type_string == std::string( "SetGridType" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is false
                json_decref( root );
                return false;
            }
            json_t * argumens_node = json_object_get( root , "argument" );
            size_t array_size = json_array_size( argumens_node );
            if ( array_size != 3 && array_size != 4 )
            {
                json_decref( root );
                return false;
            }
            json_int_t args[4];
            for ( size_t i = 0 ; i < array_size ; i++ )
            {
                json_t * argumens = json_array_get( argumens_node , i );
                if( !json_is_integer( argumens ) )
                {
                    json_decref( root );
                    return false;
                }
                args[i] = json_integer_value( argumens );
            }
            if ( array_size == 3 )
                set_grid_type( game_object , { args[0] , args[1] , args[2] } );
            else
                set_grid_type( game_object , { args[0] , args[1] , args[2] } , static_cast<GRID_TYPE>( args[3] ) );
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        //uint32_t uint32_t uint32_t uint32_t
        else if ( event_type_string == std::string( "CheckGridType" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            json_t * argumens_node = json_object_get( root , "argument" );
            size_t array_size = json_array_size( argumens_node );
            if ( array_size != 4 )
            {
                json_decref( root );
                return false;
            }
            json_int_t args[4];
            for ( size_t i = 0 ; i < sizeof( args )/sizeof( args[0] ) ; i++ )
            {
                json_t * argumens = json_array_get( argumens_node , i );
                if( !json_is_integer( argumens ) )
                {
                    json_decref( root );
                    return false;
                }
                args[i] = json_integer_value( argumens );
            }
            if ( check_grid_type( game_object , { args[0] , args[1] , args[2] } , static_cast<GRID_TYPE>( args[3] ) ) == true )
            {
                json_t * true_event_node = json_object_get( root , "true" );
                std::shared_ptr<char> true_event_json(
                    json_dumps( true_event_node , JSON_INDENT( 4 ) ),
                    []( char * event_json ){ free( event_json ); }
                );
                trigger_custom_event( game_object , true_event_json );
                json_t * new_true_event_node = json_loads( true_event_json.get() , 0 , &json_error );
                json_object_set( root , "true" , new_true_event_node );
                event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
                json_decref( new_true_event_node );
            }
            else
            {
                json_t * false_event_node = json_object_get( root , "false" );
                std::shared_ptr<char> false_event_json(
                    json_dumps( false_event_node , JSON_INDENT( 4 ) ),
                    []( char * event_json ){ free( event_json ); }
                );
                trigger_custom_event( game_object , false_event_json );
                json_t * new_false_event_node = json_loads( false_event_json.get() , 0 , &json_error );
                json_object_set( root , "false" , new_false_event_node );
                event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
                json_decref( new_false_event_node );
            }
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        else if ( event_type_string == std::string( "GameWin" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            game_win( game_object );
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        else if ( event_type_string == std::string( "GameLose" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            game_lose( game_object );
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        //uint32_t uint32_t uint32_t
        else if ( event_type_string == std::string( "MoveHero" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            json_t * argumens_node = json_object_get( root , "argument" );
            size_t array_size = json_array_size( argumens_node );
            if ( array_size != 3 )
            {
                json_decref( root );
                return false;
            }
            json_int_t args[3];
            for ( size_t i = 0 ; i < array_size ; i++ )
            {
                json_t * argumens = json_array_get( argumens_node , i );
                if( !json_is_integer( argumens ) )
                {
                    json_decref( root );
                    return false;
                }
                args[i] = json_integer_value( argumens );
            }
            move_hero( game_object , { args[0] , args[1] , args[2] } );
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        //uint32_t
        else if ( event_type_string == std::string( "GetItem" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            json_t * argumens_node = json_object_get( root , "argument" );
            size_t array_size = json_array_size( argumens_node );
            if ( array_size != 1 )
            {
                json_decref( root );
                return false;
            }
            json_t * argumens = json_array_get( argumens_node , 0 );
            json_int_t arg = json_integer_value( argumens );
            get_item( game_object , arg );
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        //uint32_t
        else if ( event_type_string == std::string( "UnlockStore" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            json_t * argumens_node = json_object_get( root , "argument" );
            size_t array_size = json_array_size( argumens_node );
            if ( array_size != 1 )
            {
                json_decref( root );
                return false;
            }
            json_t * argumens = json_array_get( argumens_node , 0 );
            json_int_t arg = json_integer_value( argumens );
            if ( game_object->store_list.size() > static_cast<std::size_t>( arg ) )
                game_object->store_list[ arg ].usability = true;
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        //uint32_t
        else if ( event_type_string == std::string( "lockStore" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            json_t * argumens_node = json_object_get( root , "argument" );
            size_t array_size = json_array_size( argumens_node );
            if ( array_size != 1 )
            {
                json_decref( root );
                return false;
            }
            json_t * argumens = json_array_get( argumens_node , 0 );
            json_int_t arg = json_integer_value( argumens );
            if ( game_object->store_list.size() > static_cast<std::size_t>( arg ) )
                game_object->store_list[ arg ].usability = false;
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        //json_object
        else if ( event_type_string == std::string( "Shopping" ) )
        {
            json_t * usability_node = json_object_get( root , "usability" ); 
            if ( json_boolean_value( usability_node ) == false )
            {
                //if node non-existent,default value is true
                json_decref( root );
                return false;
            }
            json_t * argumens_node = json_object_get( root , "argument" );
            if ( !json_is_object( argumens_node ) )
            {
                json_decref( root );
                return false;
            }
            std::shared_ptr<char> commodity_json(
                json_dumps( argumens_node , JSON_INDENT( 4 ) ),
                []( char * commodity_json ){ free( commodity_json ); }
            );
            if ( shopping( game_object , commodity_json.get() ) == false )
            {
                json_decref( root );
                return false;
            }
            json_t * trigger_limit_node = json_object_get( root , "trigger_limit" );
            if( !json_is_integer( trigger_limit_node ) )
            {
                json_decref( root );
                return true;
            }
            json_int_t trigger_limit = json_integer_value( trigger_limit_node );
            if ( trigger_limit == 1 )
            {
                json_object_set( root , "usability" , json_false() );
            }
            json_object_set( root , "trigger_limit" , json_integer( trigger_limit - 1 ) );
            event_json.reset( json_dumps( root , JSON_INDENT( 4 ) ) );
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "trigger end event content:\n%s" , event_json.get() );
        }
        else if ( event_type_string == std::string( "None" ) )
        {
            //do nothing
        }
        else
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unknown event type ignore" );
        }

        json_decref( root );
        return true;
    }

    bool shopping( struct GameEnvironment * game_object , const char * commodity_json )
    {
        json_error_t json_error;
        json_t * root = json_loads( commodity_json , 0 , &json_error );
        if ( root == NULL )
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

    void open_store_menu( struct GameEnvironment * game_object )
    {
		if ( game_object->game_status == GAME_STATUS::GAME_LOSE ||
			game_object->game_status == GAME_STATUS::GAME_WIN   ||
            game_object->game_status == GAME_STATUS::GAME_MENU  ||
            game_object->game_status == GAME_STATUS::STORE_MENU )
			return ;
        game_object->game_status = GAME_STATUS::STORE_MENU;
        GtkWidget * game_grid   = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_grid" ) );

	    GtkWidget * notebook = gtk_notebook_new();
        for( auto store : game_object->store_list )
        {
            if ( store.usability != true )
                continue;
            GtkWidget * store_label = gtk_label_new( store.name.c_str() );
	        GtkWidget * store_content = make_commodity_grid( game_object , store.content );
            gtk_notebook_append_page( GTK_NOTEBOOK( notebook ) , store_content , store_label );
        }

	    gtk_notebook_set_scrollable( GTK_NOTEBOOK( notebook ) , TRUE );
	    gtk_notebook_popup_enable( GTK_NOTEBOOK( notebook ) );

        gtk_grid_remove_column( GTK_GRID( game_grid ) , 1 );
        gtk_grid_attach( GTK_GRID( game_grid ) , notebook , 1 , 0 , 1 , 1 );
        gtk_widget_set_size_request( GTK_WIDGET( notebook ) , ( game_object->towers.LENGTH )*MagicTower::TOWER_GRID_SIZE ,  ( game_object->towers.WIDTH )*MagicTower::TOWER_GRID_SIZE );
        gtk_widget_show_all( notebook );
    }

    void close_store_menu( struct GameEnvironment * game_object )
    {
		if ( game_object->game_status != GAME_STATUS::STORE_MENU )
			return ;
        GtkWidget * game_grid   = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_grid" ) );
        GtkWidget * tower_area  = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );

        gtk_grid_remove_column( GTK_GRID( game_grid ) , 1 );
        gtk_grid_attach( GTK_GRID( game_grid ) , tower_area , 1 , 0 , 1 , 1 );
        gtk_widget_show( game_grid );
        game_object->game_status = GAME_STATUS::NORMAL;
    }

    void open_game_menu( struct GameEnvironment * game_object )
    {
		if ( game_object->game_status == GAME_STATUS::GAME_LOSE ||
			game_object->game_status == GAME_STATUS::GAME_WIN   || 
            game_object->game_status == GAME_STATUS::GAME_MENU  ||
            game_object->game_status == GAME_STATUS::STORE_MENU )
			return ;
		game_object->game_status = GAME_STATUS::GAME_MENU;
        GtkWidget * game_window    = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_window" ) );
        GtkWidget * game_grid      = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_grid" ) );
        GtkWidget * game_menu  = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_menu" ) );
        gtk_widget_set_size_request( GTK_WIDGET( game_menu ) , ( game_object->towers.LENGTH + game_object->towers.LENGTH/2 )*MagicTower::TOWER_GRID_SIZE ,  ( game_object->towers.WIDTH )*MagicTower::TOWER_GRID_SIZE );
        gtk_container_remove( GTK_CONTAINER( game_window ) , game_grid );
        gtk_container_add( GTK_CONTAINER( game_window ) , game_menu );
        gtk_widget_show( game_menu );
    }

    void game_win( struct GameEnvironment * game_object )
    {
		if ( game_object->game_status == GAME_STATUS::GAME_LOSE ||
			 game_object->game_status == GAME_STATUS::GAME_WIN  || 
             game_object->game_status == GAME_STATUS::GAME_MENU ||
             game_object->game_status == GAME_STATUS::STORE_MENU )
			return ;
		game_object->game_status = GAME_STATUS::GAME_WIN;
        GtkWidget * game_window      = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_window" ) );
        GtkWidget * game_grid        = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_grid" ) );
        GtkWidget * game_end_menu    = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_end_menu" ) );
        GtkWidget * story_text_label = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "story_text_label" ) );
        gtk_label_set_label( GTK_LABEL( story_text_label ) , "恭喜通关" );
        gtk_container_remove( GTK_CONTAINER( game_window ) , game_grid );
        gtk_container_add( GTK_CONTAINER( game_window ) , game_end_menu );
        gtk_widget_show( game_end_menu );
    }

    void game_lose( struct GameEnvironment * game_object )
    {
		if ( game_object->game_status == GAME_STATUS::GAME_LOSE ||
			game_object->game_status == GAME_STATUS::GAME_WIN   || 
            game_object->game_status == GAME_STATUS::GAME_MENU  ||
            game_object->game_status == GAME_STATUS::STORE_MENU )
			return ;
		game_object->game_status = GAME_STATUS::GAME_LOSE;
        GtkWidget * game_window = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_window" ) );
        GtkWidget * game_grid = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_grid" ) );
        GtkWidget * game_end_menu = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_end_menu" ) );
        GtkWidget * story_text_label = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "story_text_label" ) );
        gtk_label_set_label( GTK_LABEL( story_text_label ) , "游戏失败" );
        gtk_container_remove( GTK_CONTAINER( game_window ) , game_grid );
        gtk_container_add( GTK_CONTAINER( game_window ) , game_end_menu );
        gtk_widget_show( game_end_menu );
    }

    static std::int64_t get_combat_damage_of_last( Hero& hero , Monster& monster )
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

    static std::int64_t get_combat_damage_of_normal( Hero& hero , Monster& monster )
    {
        if ( hero.level > monster.level )
            return get_combat_damage_of_last( hero , monster );
        else
            return get_combat_damage_of_first( hero , monster );
        return -1;
    }

    static std::int64_t get_combat_damage_of_first( Hero& hero , Monster& monster )
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

    static std::int64_t get_combat_damage_of_double( Hero& hero , Monster& monster )
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

    static gboolean do_shopping( GtkWidget * widget , gpointer data )
    {
        const char * commodity_json = gtk_widget_get_name( widget );
        shopping( static_cast<MagicTower::GameEnvironment *>( data ) , commodity_json );
        return TRUE;
    }

    std::string deserialize_commodity_content( const char * content )
    {
        json_error_t json_error;
        json_t * root = json_loads( content , 0 , &json_error );
        if ( root == NULL )
        {
            json_decref( root );
            return std::string( "" );
        }

        json_t * price_type_node = json_object_get( root , "price_type" );
        if ( !json_is_string( price_type_node ) )
        {
            json_decref( root );
            return std::string( "" );
        }
        std::string price_type( json_string_value( price_type_node ) );
        json_t * price_node = json_object_get( root , "price" );
        if ( !json_is_integer( price_node ) )
        {
            json_decref( root );
            return std::string( "" );
        }
        json_int_t price_value = json_integer_value( price_node );
        json_t * commodity_type_node = json_object_get( root , "commodity_type" );
        if ( !json_is_string( commodity_type_node ) )
        {
            json_decref( root );
            return std::string( "" );
        }
        std::string commodity_type( json_string_value( commodity_type_node ) );
        json_t * commodity_value_node = json_object_get( root , "commodity_value" );
        if ( !json_is_integer( commodity_value_node ) )
        {
            json_decref( root );
            return std::string( "" );
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
                return std::string( "" );
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
            return std::string( "" );
        }

        return deserialize_string;
    }

    static GtkWidget * make_commodity_grid( struct GameEnvironment * game_object , std::string content )
    {
        json_error_t json_error;
        json_t * root = json_loads( content.c_str() , 0 , &json_error );
        GtkWidget * commodity_grid = gtk_grid_new();
        gtk_grid_set_row_homogeneous( GTK_GRID( commodity_grid ) , TRUE );
        gtk_grid_set_column_homogeneous( GTK_GRID( commodity_grid ) , TRUE );
        //gtk_widget_set_size_request( GTK_WIDGET( commodity_grid ) , ( game_object->towers.LENGTH )*TOWER_GRID_SIZE ,  ( game_object->towers.WIDTH )*TOWER_GRID_SIZE );
        if ( root == NULL )
        {
            json_decref( root );
            return commodity_grid;
        }
        json_t * commodity_list = json_object_get( root , "commoditys" );
        if ( !json_is_array( commodity_list ) )
        {
            json_decref( root );
            return commodity_grid;
        }
        size_t commodity_size = json_array_size( commodity_list );
        for( size_t i = 0 ; i < commodity_size ; i++ )
        {
            json_t * commodity_node = json_array_get( commodity_list , i );
            if ( !json_is_object( commodity_node ) )
            {
                json_decref( root );
                return commodity_grid;
            }
            const char * commodity_content = json_dumps( commodity_node , JSON_INDENT( 4 ) );
            std::string button_label = deserialize_commodity_content( commodity_content );
            GtkWidget * commodity_button = gtk_button_new_with_label( button_label.c_str() );
            gtk_widget_set_name( commodity_button , commodity_content );
            g_signal_connect( G_OBJECT( commodity_button ) , "clicked" , G_CALLBACK( do_shopping ) , game_object );
            gtk_grid_attach( GTK_GRID( commodity_grid ) , commodity_button , 1 , i , 1 , 1 );
        }
        json_decref( root );
        return commodity_grid;
    }

}
