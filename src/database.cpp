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
    DataBase::DataBase( std::string filename ):
        db_filename(filename)
    {
        this->sqlite3_error_code = sqlite3_open_v2( this->db_filename.c_str() , &( this->db_handler ) 
            , SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE , nullptr );
        if ( sqlite3_error_code != SQLITE_OK )
            throw sqlite_open_failure( sqlite3_error_code , std::string( this->db_filename.c_str() ) );
        create_tables();
    }

    DataBase::~DataBase()
    {
        sqlite3_exec( this->db_handler , "VACUUM" , nullptr , nullptr , nullptr );
        this->sqlite3_error_code = sqlite3_close( db_handler );
        while( this->sqlite3_error_code == SQLITE_BUSY )
        {
            this->sqlite3_error_code = SQLITE_OK;
            this->sql_statement_handler = sqlite3_next_stmt( this->db_handler , nullptr );
            if ( this->sql_statement_handler != nullptr )
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
                CREATE TABLE IF NOT EXISTS access_layers (
                    layer INTEGER PRIMARY KEY AUTOINCREMENT
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS script_flags (
                    flag_id     INTEGER PRIMARY KEY AUTOINCREMENT,
                    flag_name   TEXT,
                    flag_value  INT (32)
                );
            )"
        };
        for ( size_t i = 0 ; i < sizeof( create_table_sqls )/sizeof( const char * ) ; i++ )
            sqlite3_exec( this->db_handler , create_table_sqls[i] , nullptr , nullptr , nullptr );
    }

    Hero DataBase::get_hero_info( std::size_t archive_id )
    {
        Hero hero;
        const char sql_statement[] = "SELECT layers,x,y,level,life,attack,defense,gold,experience,yellow_key,"
        "blue_key,red_key FROM hero WHERE id = ?";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler , sql_statement
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_int_failure( 1 , archive_id , this->sqlite3_error_code , std::string( sql_statement ) );
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

        const char sql_statement[] = "SELECT length,width,height,content FROM tower WHERE id = ?";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler , sql_statement
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        this->sqlite3_error_code = sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_int_failure( 1 , archive_id , this->sqlite3_error_code , std::string( sql_statement ) );
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

    std::map<std::uint32_t , bool> DataBase::get_access_layers()
    {
        std::map<std::uint32_t , bool> access_layers;
        std::uint32_t layer = 0;
        const char sql_statement[] = "SELECT layer FROM access_layers";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler , sql_statement
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 1 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw sqlite_table_format_failure( std::string( sql_statement ) );
            }

            layer = sqlite3_column_int( this->sql_statement_handler , 0 );
            access_layers[ layer ] = true;
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

        return access_layers;
    }

    std::map<std::string , std::uint32_t> DataBase::get_script_flags()
    {
        std::map<std::string , std::uint32_t> script_flags;
        const char sql_statement[] = "SELECT flag_name , flag_value FROM script_flags";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler , sql_statement
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );
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

            std::string flag_name( reinterpret_cast< const char * >( sqlite3_column_text( this->sql_statement_handler , 0 ) ) );
            std::uint32_t flag_value = sqlite3_column_int( this->sql_statement_handler , 1 );
            script_flags[flag_name] = flag_value;
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

        return script_flags;
    }

/*  
sqlite document:
    The initial "INSERT" keyword can be replaced by "REPLACE" or "INSERT OR action" to specify an alternative constraint conflict
    resolution algorithm to use during that one INSERT command. For compatibility with MySQL,
    the parser allows the use of the single keyword REPLACE as an alias for "INSERT OR REPLACE".
*/

    void DataBase::set_hero_info( const Hero& hero , std::size_t archive_id )
    {
        const char sql_statement[] = "INSERT OR REPLACE INTO hero(id,layers,x,y,level,life,attack,defense,"
            "gold,experience,yellow_key,blue_key,red_key) VALUES( ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

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
        const char sql_statement[] = "INSERT OR REPLACE INTO tower(id,length,width,height,content) VALUES( ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

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

    void DataBase::set_access_layers( std::map<std::uint32_t , bool>& maps )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , nullptr , nullptr , nullptr );

        const char sql_statement[] = "INSERT OR REPLACE INTO access_layers(layer) VALUES( ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_prepare_statement_failure( this->sqlite3_error_code , std::string( sql_statement ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 1 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw sqlite_bind_count_failure( std::string( sql_statement ) );
        }

        for( auto& layer : maps )
        {
            if ( layer.second == false )
                continue;
            sqlite3_bind_int64( this->sql_statement_handler , 1 , layer.first );

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
        
        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , nullptr , nullptr , nullptr );
        
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }

    void DataBase::set_script_flags( std::map<std::string , std::uint32_t>& flags )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , nullptr , nullptr , nullptr );

        const char sql_statement[] = "INSERT OR REPLACE INTO script_flags(flag_name,flag_value) VALUES( ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

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

        for( auto& flag : flags )
        {
            sqlite3_bind_text( this->sql_statement_handler , 1 , flag.first.c_str() , flag.first.size() , SQLITE_STATIC );
            sqlite3_bind_int64( this->sql_statement_handler , 2 , flag.second );

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
        
        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , nullptr , nullptr , nullptr );
        
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw sqlite_finalize_statement_failure( this->sqlite3_error_code , sql_statement );
        }
    }
}
