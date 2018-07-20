#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cinttypes>

#include <string>

#include <sqlite3.h>

#include "database.h"
#include "hero.h"
#include "item.h"
#include "monster.h"
#include "tower.h"

namespace MagicTower
{
    DataBase::DataBase( const char * filename )
    {
        this->db_filename = std::string( filename );
        this->sqlite3_error_code = sqlite3_open_v2( this->db_filename.c_str() , &( this->db_handler ) 
            , SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE , NULL );
        if ( sqlite3_error_code != SQLITE_OK )
            throw sqlite_open_failure( sqlite3_error_code , std::string( this->db_filename.c_str() ) );
        create_tables();
    }

    DataBase::~DataBase()
    {
        this->sqlite3_error_code = sqlite3_close( db_handler );
        while( this->sqlite3_error_code == SQLITE_BUSY )
        {
            this->sqlite3_error_code = SQLITE_OK;
            this->sql_statement_handler = sqlite3_next_stmt( this->db_handler , NULL );
            if ( this->sql_statement_handler != NULL )
            {
                this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
                if ( this->sqlite3_error_code == SQLITE_OK )
                    this->sqlite3_error_code = sqlite3_close( db_handler );
            }
        }
        if ( sqlite3_error_code != SQLITE_OK )
        {
            exit( sqlite3_error_code );
        }
    }

    void DataBase::create_tables()
    {
        const char * create_table_sqls[] = 
        {
            R"(
                CREATE TABLE IF NOT EXISTS tower (
                    id      INTEGER  PRIMARY KEY AUTOINCREMENT,
                    length  INT (32),
                    width   INT (32),
                    height  INT (32),
                    content BLOB
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS stores (
                    id        INTEGER PRIMARY KEY AUTOINCREMENT,
                    usability BOOLEAN DEFAULT (0),
                    name      TEXT,
                    content   TEXT
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS stairs (
                    id     INTEGER  PRIMARY KEY AUTOINCREMENT,
                    type   INT (32),
                    layers INT (32),
                    x      INT (32),
                    y      INT (32) 
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS monster (
                    id         INTEGER  PRIMARY KEY AUTOINCREMENT,
                    type       INT (32),
                    type_value INT (32),
                    name       TEXT,
                    level      INT (32),
                    life       INT (32),
                    attack     INT (32),
                    defense    INT (32),
                    gold       INT (32),
                    experience INT (32) 
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS item (
                    id    INTEGER  PRIMARY KEY AUTOINCREMENT,
                    name  TEXT,
                    type  INT (32),
                    value INT (32) 
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS hero (
                    id         INTEGER  PRIMARY KEY AUTOINCREMENT,
                    layers     INT (32),
                    x          INT (32),
                    y          INT (32),
                    level      INT (32),
                    life       INT (32),
                    attack     INT (32),
                    defense    INT (32),
                    gold       INT (32),
                    experience INT (32),
                    yellow_key INT (32),
                    blue_key   INT (32),
                    red_key    INT (32) 
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS events (
                    id             INTEGER PRIMARY KEY AUTOINCREMENT,
                    event_position TEXT,
                    event_content  TEXT
                );
            )"
        };
        for ( size_t i = 0 ; i < sizeof( create_table_sqls )/sizeof( const char * ) ; i++ )
            sqlite3_exec( this->db_handler , create_table_sqls[i] , NULL , NULL , NULL );
    }

    Hero DataBase::get_hero_info( std::size_t archive_id )
    {
        Hero hero;
        char sql_statement[] = "SELECT layers,x,y,level,life,attack,defense,gold,experience,yellow_key,"
        "blue_key,red_key FROM hero WHERE id = ?";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler ,  sql_statement 
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_int_failure( 1 ,archive_id , this->sqlite3_error_code , std::string( sql_statement ) );
        }

        //id should be unique,so hero will not be repeat setting
        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 12 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }
            hero.layers = sqlite3_column_int( this->sql_statement_handler , 0 );
            hero.x = sqlite3_column_int( this->sql_statement_handler , 1 );
            hero.y = sqlite3_column_int( this->sql_statement_handler , 2 );
            hero.level = sqlite3_column_int( this->sql_statement_handler , 3 );
            hero.life = sqlite3_column_int( this->sql_statement_handler , 4 );
            hero.attack = sqlite3_column_int( this->sql_statement_handler , 5 );
            hero.defense = sqlite3_column_int( this->sql_statement_handler , 6 );
            hero.gold = sqlite3_column_int( this->sql_statement_handler , 7 );
            hero.experience = sqlite3_column_int( this->sql_statement_handler , 8 );
            hero.yellow_key = sqlite3_column_int( this->sql_statement_handler , 9 );
            hero.blue_key = sqlite3_column_int( this->sql_statement_handler , 10 );
            hero.red_key = sqlite3_column_int( this->sql_statement_handler , 11 );
        }

        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
        return hero;
    }

    Tower DataBase::get_tower_info( std::size_t archive_id )
    {
        std::uint32_t HEIGHT = 0;
        std::uint32_t LENGTH = 0;
        std::uint32_t WIDTH = 0;
        std::vector<struct TowerGrid> maps;

        char sql_statement[] = "SELECT length,width,height,content FROM tower WHERE id = ?";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler ,  sql_statement 
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_int_failure( 1 ,archive_id , this->sqlite3_error_code , std::string( sql_statement ) );
        }

        //id should be unique,so hero will not be repeat setting
        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 4 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }
            LENGTH = sqlite3_column_int( this->sql_statement_handler , 0 );
            WIDTH = sqlite3_column_int( this->sql_statement_handler , 1 );
            HEIGHT = sqlite3_column_int( this->sql_statement_handler , 2 );
            //default vector size is 0,resize to >= maps size
            //data size is agreed in advance,so don't call sqlite3_column_bytes.
            struct TowerGrid * data = reinterpret_cast<struct TowerGrid *>( const_cast<void *>( sqlite3_column_blob( this->sql_statement_handler , 3 ) ) );
            std::vector<struct TowerGrid> temp( data + 0 , data + HEIGHT*LENGTH*WIDTH );
            maps.swap( temp );
        }
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
        Tower tower = { HEIGHT , LENGTH , WIDTH , maps };
        return tower;
    }

    std::map<event_position_t , std::string > DataBase::get_custom_events()
    {
        std::map<event_position_t , std::string > events;
        const char sql_statement[] = "SELECT event_position,event_content FROM events";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler ,  sql_statement 
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 2 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }

            const char * temp_event_position = reinterpret_cast< const char * >( 
                sqlite3_column_text( this->sql_statement_handler , 0 ) );
            std::string event_json_string( reinterpret_cast< const char * >(
                sqlite3_column_text( this->sql_statement_handler , 1 ) ) );
            
            std::uint32_t x , y , z;
            sscanf( temp_event_position , "%" PRIu32 ",%" PRIu32 ",%" PRIu32 , &x , &y , &z );
            event_position_t position = { x , y , z };

            events[ position ] = event_json_string;
        }
        
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
        return events;
    }

    std::vector<Item> DataBase::get_item_list()
    {
        std::vector<Item> items;
        const char sql_statement[] = "SELECT id,name,type,value FROM item";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler ,  sql_statement 
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 4 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }

            std::size_t id = static_cast< std::size_t >( sqlite3_column_int( this->sql_statement_handler , 0 ) );
            const char * temp_item_name = reinterpret_cast< const char * >( 
                sqlite3_column_text( this->sql_statement_handler , 1 ) );
            ITEM_TYPE temp_item_ability_type = static_cast< ITEM_TYPE >(
                sqlite3_column_int( this->sql_statement_handler , 2 ) );
            std::int32_t temp_item_ability_value = sqlite3_column_int( this->sql_statement_handler , 3 );

            //C API return NULL,not return nullptr
            if ( temp_item_name == NULL )
                temp_item_name = "";

            items.push_back({
                id , temp_item_name , temp_item_ability_type , temp_item_ability_value
            });
        }
        
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
        return items;
    }

    std::vector<Monster> DataBase::get_monster_list()
    {
        std::vector<Monster> monsters;
        const char sql_statement[] = "SELECT id,type,type_value,name,level,life,attack,defense,gold,experience FROM monster";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler ,  sql_statement 
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 10 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }

            std::size_t id = static_cast< std::size_t >( sqlite3_column_int( this->sql_statement_handler , 0 ) );
            ATTACK_TYPE type = static_cast< ATTACK_TYPE >( sqlite3_column_int( this->sql_statement_handler , 1 ) );
            std::uint32_t type_value = sqlite3_column_int( this->sql_statement_handler , 2 );
            const char * name = reinterpret_cast< const char * >( 
                sqlite3_column_text( this->sql_statement_handler , 3 ) );
            std::uint32_t level = sqlite3_column_int( this->sql_statement_handler , 4 ); 
            std::uint32_t life = sqlite3_column_int( this->sql_statement_handler , 5 );
            std::uint32_t attack = sqlite3_column_int( this->sql_statement_handler , 6 );
            std::uint32_t defense = sqlite3_column_int( this->sql_statement_handler , 7 );
            std::uint32_t gold = sqlite3_column_int( this->sql_statement_handler , 8 );
            std::uint32_t experience = sqlite3_column_int( this->sql_statement_handler , 9 );
            monsters.push_back({
                id , type , type_value , name , level , life , attack , defense , gold , experience
            });
        }
        
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
        return monsters;
    }

    std::vector<Stairs> DataBase::get_stairs_list()
    {
        std::vector<Stairs> stairs;
        const char sql_statement[] = "SELECT id,type,layers,x,y FROM stairs";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler ,  sql_statement 
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 5 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }

            std::size_t id = static_cast< std::size_t >( sqlite3_column_int( this->sql_statement_handler , 0 ) );
            std::int32_t type = sqlite3_column_int( this->sql_statement_handler , 1 );
            std::uint32_t layers = sqlite3_column_int( this->sql_statement_handler , 2 );
            std::uint32_t x = sqlite3_column_int( this->sql_statement_handler , 3 );
            std::uint32_t y = sqlite3_column_int( this->sql_statement_handler , 4 );
            stairs.push_back({ id , type , layers , x , y });
        }
        
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
        return stairs;
    }

    std::vector<Store> DataBase::get_store_list()
    {
        std::vector<Store> stores;
        const char sql_statement[] = "SELECT usability,name,content FROM stores";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler ,  sql_statement 
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 3 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }

            bool usability = sqlite3_column_int( this->sql_statement_handler , 0 );
            std::string name( reinterpret_cast< const char * >( sqlite3_column_text( this->sql_statement_handler , 1 ) ) );
            std::string content( reinterpret_cast< const char * >( sqlite3_column_text( this->sql_statement_handler , 2 ) ) );
            stores.push_back({ usability , name , content });
        }
        
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
        return stores;
    }

/*  
sqlite document:
    The initial "INSERT" keyword can be replaced by "REPLACE" or "INSERT OR action" to specify an alternative constraint conflict
    resolution algorithm to use during that one INSERT command. For compatibility with MySQL,
    the parser allows the use of the single keyword REPLACE as an alias for "INSERT OR REPLACE".
*/

    void DataBase::set_hero_info( const Hero& hero , std::size_t archive_id )
    {
        char sql_statement[] = "INSERT OR REPLACE INTO hero(id,layers,x,y,level,life,attack,defense,"
            "gold,experience,yellow_key,blue_key,red_key) VALUES( ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 13 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        sqlite3_bind_int( this->sql_statement_handler , 2 , hero.layers );
        sqlite3_bind_int( this->sql_statement_handler , 3 , hero.x );
        sqlite3_bind_int( this->sql_statement_handler , 4 , hero.y );
        sqlite3_bind_int( this->sql_statement_handler , 5 , hero.level );
        sqlite3_bind_int( this->sql_statement_handler , 6 , hero.life );
        sqlite3_bind_int( this->sql_statement_handler , 7 , hero.attack );
        sqlite3_bind_int( this->sql_statement_handler , 8 , hero.defense );
        sqlite3_bind_int( this->sql_statement_handler , 9 , hero.gold );
        sqlite3_bind_int( this->sql_statement_handler , 10 , hero.experience );
        sqlite3_bind_int( this->sql_statement_handler , 11 , hero.yellow_key );
        sqlite3_bind_int( this->sql_statement_handler , 12 , hero.blue_key );
        sqlite3_bind_int( this->sql_statement_handler , 13 , hero.red_key );


        //UPDATE not return data so sqlite3_step not return SQLITE_ROW
        this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }

    void DataBase::set_tower_info( const Tower& tower , std::size_t archive_id )
    {
        char sql_statement[] = "INSERT OR REPLACE INTO tower(id,length,width,height,content) VALUES( ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 5 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        sqlite3_bind_int( this->sql_statement_handler , 2 , tower.LENGTH );
        sqlite3_bind_int( this->sql_statement_handler , 3 , tower.WIDTH );
        sqlite3_bind_int( this->sql_statement_handler , 4 , tower.HEIGHT );
        sqlite3_bind_blob( this->sql_statement_handler , 5 , tower.maps.data() ,
            sizeof( MagicTower::TowerGrid )*tower.LENGTH*tower.WIDTH*tower.HEIGHT , SQLITE_STATIC );


        //UPDATE not return data so sqlite3_step not return SQLITE_ROW
        this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }

    void DataBase::set_item_list( const std::vector<Item>& items )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , NULL , NULL , NULL );

        const char sql_statement[] = "INSERT OR REPLACE INTO item(id,name"
            ",type,value) VALUES( ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 4 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        for( auto& item : items )
        {
            sqlite3_bind_int64( this->sql_statement_handler , 1 , item.item_id );
            sqlite3_bind_text( this->sql_statement_handler , 2 , item.item_name.c_str() , item.item_name.size() , SQLITE_STATIC );
            sqlite3_bind_int( this->sql_statement_handler , 3 , item.ability_type );
            sqlite3_bind_int( this->sql_statement_handler , 4 , item.ability_value );

            //UPDATE or INSERT not return data so sqlite3_step not return SQLITE_ROW
            this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_DONE )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_reset_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }

        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , NULL , NULL , NULL );

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }

    void DataBase::set_monster_list( const std::vector<Monster>& monsters )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , NULL , NULL , NULL );

        const char sql_statement[] = "INSERT OR REPLACE INTO monster(id,type,type_value,name"
            ",level,life,attack,defense,gold,experience) VALUES( ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 10 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        for( auto& monster : monsters )
        {
            sqlite3_bind_int64( this->sql_statement_handler , 1 , monster.id );
            sqlite3_bind_int( this->sql_statement_handler , 2 , monster.type );
            sqlite3_bind_int( this->sql_statement_handler , 3 , monster.type_value );
            sqlite3_bind_text( this->sql_statement_handler , 4 , monster.name.c_str() , monster.name.size() , SQLITE_STATIC );
            sqlite3_bind_int( this->sql_statement_handler , 5 , monster.level );
            sqlite3_bind_int( this->sql_statement_handler , 6 , monster.life );
            sqlite3_bind_int( this->sql_statement_handler , 7 , monster.attack );
            sqlite3_bind_int( this->sql_statement_handler , 8 , monster.defense );
            sqlite3_bind_int( this->sql_statement_handler , 9 , monster.gold );
            sqlite3_bind_int( this->sql_statement_handler , 10 , monster.experience );

            //UPDATE or INSERT not return data so sqlite3_step not return SQLITE_ROW
            this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_DONE )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_reset_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }

        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , NULL , NULL , NULL );

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }

    void DataBase::set_stairs_list( const std::vector<Stairs>& stairs )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , NULL , NULL , NULL );

        const char sql_statement[] = "INSERT OR REPLACE INTO stairs(id,type,layers,x,y) VALUES( ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 5 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        for( auto& stair : stairs )
        {
            sqlite3_bind_int64( this->sql_statement_handler , 1 , stair.id );
            sqlite3_bind_int( this->sql_statement_handler , 2 , stair.type );
            sqlite3_bind_int( this->sql_statement_handler , 3 , stair.layers );
            sqlite3_bind_int( this->sql_statement_handler , 4 , stair.x );
            sqlite3_bind_int( this->sql_statement_handler , 5 , stair.y );

            //UPDATE or INSERT not return data so sqlite3_step not return SQLITE_ROW
            this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_DONE )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_reset_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }

        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , NULL , NULL , NULL );

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }

    void DataBase::set_store_list( std::vector<Store>& stores )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , NULL , NULL , NULL );

        const char sql_statement[] = "INSERT OR REPLACE INTO stores(usability,name,content) VALUES( ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 3 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        for( auto& store : stores )
        {
            sqlite3_bind_int( this->sql_statement_handler , 1 , store.usability );
            sqlite3_bind_text( this->sql_statement_handler , 2 , store.name.c_str() , store.name.size()  , SQLITE_STATIC );
            sqlite3_bind_text( this->sql_statement_handler , 3 , store.content.c_str() , store.content.size() , SQLITE_STATIC );

            //UPDATE or INSERT not return data so sqlite3_step not return SQLITE_ROW
            this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_DONE )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_reset_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }

        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , NULL , NULL , NULL );

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }

    void DataBase::set_custom_events( std::map<event_position_t , std::string>& custom_events )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , NULL , NULL , NULL );

        const char sql_statement[] = "INSERT OR REPLACE INTO events(event_position,event_content) VALUES( ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , NULL );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 2 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        for( auto& custom_event : custom_events )
        {
            std::uint32_t x = std::get<0>( custom_event.first );
            std::uint32_t y = std::get<1>( custom_event.first );
            std::uint32_t z = std::get<2>( custom_event.first );
            std::string position_str;
            position_str += std::to_string( x ) + std::string( "," );
            position_str += std::to_string( y ) + std::string( "," );
            position_str += std::to_string( z );
            sqlite3_bind_text( this->sql_statement_handler , 1 , position_str.c_str() , position_str.size() , SQLITE_STATIC );
            sqlite3_bind_text( this->sql_statement_handler , 2 , custom_event.second.c_str() , custom_event.second.size() , SQLITE_STATIC );

            //UPDATE or INSERT not return data so sqlite3_step not return SQLITE_ROW
            this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_DONE )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_evaluate_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_reset_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }
        
        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , NULL , NULL , NULL );
        
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }
}
