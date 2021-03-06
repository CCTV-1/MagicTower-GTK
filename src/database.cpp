#include <cstddef>
#include <cinttypes>

#include <optional>
#include <stdexcept>
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
        if (  this->sqlite3_error_code != SQLITE_OK )
            throw std::runtime_error( std::string( "open file:" ) + filename + std::string( " failure,slite3 error code:" ) + std::to_string(  this->sqlite3_error_code ) );
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
                CREATE TABLE IF NOT EXISTS towerfloor (
                    id                  INT (32),
                    length              INT (32),
                    width               INT (32),
                    default_floorid     INT (32),
                    tp_x                INT (32),
                    tp_y                INT (32),
                    fv_x                INT (32),
                    fv_y                INT (32),
                    name                TEXT,
                    content             BLOB
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS hero (
                    id         INTEGER  PRIMARY KEY AUTOINCREMENT,
                    floors     INT (32),
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
                    red_key    INT (32),
                    direction  INT (32)
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS script_flags (
                    flag_name   TEXT,
                    flag_value  INT (32)
                );
            )",
            R"(
                CREATE TABLE IF NOT EXISTS inventories (
                    item_id      INTEGER  PRIMARY KEY AUTOINCREMENT,
                    item_number  INT(32)
                );
            )"
        };
        for ( size_t i = 0 ; i < sizeof( create_table_sqls )/sizeof( const char * ) ; i++ )
            sqlite3_exec( this->db_handler , create_table_sqls[i] , nullptr , nullptr , nullptr );
    }

    Hero DataBase::get_hero_info( std::size_t archive_id )
    {
        Hero hero;
        const char sql_statement[] = "SELECT floors,x,y,level,life,attack,defense,gold,experience,yellow_key,"
        "blue_key,red_key,direction FROM hero WHERE id = ?";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler , sql_statement
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        this->sqlite3_error_code = sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "bind statement:\"" ) + std::string( sql_statement ) + std::string( "\" failure,sqlite error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        //id should be unique,so hero will not be repeat setting
        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 13 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( sql_statement ) );
            }
            hero.floors = sqlite3_column_int( this->sql_statement_handler , 0 );
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
            hero.direction = static_cast<DIRECTION>( sqlite3_column_int( this->sql_statement_handler , 12 ) );
        }

        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        return hero;
    }

    TowerMap DataBase::get_tower_info()
    {
        const char sql_statement[] = "SELECT id,length,width,default_floorid,tp_x,tp_y,fv_x,fv_y,name,content FROM towerfloor";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler , sql_statement
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        TowerMap towers = {};
        //id should be unique,so hero will not be repeat setting
        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 10 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( sql_statement ) );
            }

            std::uint32_t floor_id = sqlite3_column_int( this->sql_statement_handler , 0 );
            std::uint32_t floor_length = sqlite3_column_int( this->sql_statement_handler , 1 );
            std::uint32_t floor_width = sqlite3_column_int( this->sql_statement_handler , 2 );
            std::uint32_t default_floorid = sqlite3_column_int( this->sql_statement_handler , 3 );
            decltype( TowerFloor::teleport_point ) tp_point;
            if ( ( sqlite3_column_type( this->sql_statement_handler , 4 ) == SQLITE_NULL ) || 
                 ( sqlite3_column_type( this->sql_statement_handler , 5 ) == SQLITE_NULL )
            )
            {
                tp_point = std::nullopt;
            }
            else
            {
                tp_point = { sqlite3_column_int( this->sql_statement_handler , 4 ) , sqlite3_column_int( this->sql_statement_handler , 5 ) };
            }
            decltype( TowerFloor::field_vision ) field_vision;
            if ( ( sqlite3_column_type( this->sql_statement_handler , 6 ) == SQLITE_NULL ) || 
                 ( sqlite3_column_type( this->sql_statement_handler , 7 ) == SQLITE_NULL )
            )
            {
                field_vision = std::nullopt;
            }
            else
            {
                field_vision = { sqlite3_column_int( this->sql_statement_handler , 6 ) , sqlite3_column_int( this->sql_statement_handler , 7 ) };
            }
            std::string floor_name( reinterpret_cast< const char * >( sqlite3_column_text( this->sql_statement_handler , 8 ) ) );
            //default vector size is 0,resize to >= maps size
            //data size is agreed in advance,so don't call sqlite3_column_bytes.
            struct TowerGrid * data = reinterpret_cast<struct TowerGrid *>( const_cast<void *>( sqlite3_column_blob( this->sql_statement_handler , 9 ) ) );
            decltype( TowerFloor::content ) temp( data + 0 , data + floor_length*floor_width );
            towers.map[floor_id].length = floor_length;
            towers.map[floor_id].width = floor_width;
            towers.map[floor_id].default_floorid = default_floorid;
            towers.map[floor_id].teleport_point = tp_point;
            towers.map[floor_id].field_vision = field_vision;
            towers.map[floor_id].name = floor_name;
            towers.map[floor_id].content = temp;
        }
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        return towers;
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
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 2 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( sql_statement ) );
            }

            std::string flag_name( reinterpret_cast< const char * >( sqlite3_column_text( this->sql_statement_handler , 0 ) ) );
            std::uint32_t flag_value = sqlite3_column_int( this->sql_statement_handler , 1 );
            script_flags[flag_name] = flag_value;
        }
        
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        return script_flags;
    }

    std::map<std::uint32_t , std::uint32_t> DataBase::get_inventories()
    {
        std::map<std::uint32_t , std::uint32_t> inventories;
        const char sql_statement[] = "SELECT item_id , item_number FROM inventories";
        this->sqlite3_error_code = sqlite3_prepare_v2( db_handler , sql_statement
            , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        while ( ( this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler ) ) == SQLITE_ROW )
        {
            if ( sqlite3_column_count( this->sql_statement_handler ) != 2 )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( sql_statement ) );
            }

            std::uint32_t item_id = sqlite3_column_int( this->sql_statement_handler , 0 );
            std::uint32_t item_number = sqlite3_column_int( this->sql_statement_handler , 1 );
            inventories[item_id] = item_number;
        }
        
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        return inventories;
    }

    /*  
    sqlite document:
        The initial "INSERT" keyword can be replaced by "REPLACE" or "INSERT OR action" to specify an alternative constraint conflict
        resolution algorithm to use during that one INSERT command. For compatibility with MySQL,
        the parser allows the use of the single keyword REPLACE as an alias for "INSERT OR REPLACE".
    */

    void DataBase::set_hero_info( const Hero& hero , std::size_t archive_id )
    {
        const char sql_statement[] = "INSERT OR REPLACE INTO hero(id,floors,x,y,level,life,attack,defense,"
            "gold,experience,yellow_key,blue_key,red_key,direction) VALUES( ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 14 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "sql statement:\"" ) + std::string( sql_statement ) + std::string( "\" bind argument count out of expectation" ) );
        }

        sqlite3_bind_int( this->sql_statement_handler , 1 , archive_id );
        sqlite3_bind_int( this->sql_statement_handler , 2 , hero.floors );
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
        sqlite3_bind_int( this->sql_statement_handler , 14 , static_cast<int>( hero.direction ) );

        //UPDATE not return data so sqlite3_step not return SQLITE_ROW
        this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_DONE )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
    }

    void DataBase::set_tower_info( const TowerMap& tower )
    {
        const char sql_statement[] = "INSERT OR REPLACE INTO towerfloor(id,length,width,default_floorid,tp_x,tp_y,fv_x,fv_y,name,content) VALUES( ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 10 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "sql statement:\"" ) + std::string( sql_statement ) + std::string( "\" bind argument count out of expectation" ) );
        }

        for ( auto& floor : tower.map )
        {
            sqlite3_bind_int( this->sql_statement_handler , 1 , floor.first );
            sqlite3_bind_int( this->sql_statement_handler , 2 , floor.second.length );
            sqlite3_bind_int( this->sql_statement_handler , 3 , floor.second.width );
            sqlite3_bind_int( this->sql_statement_handler , 4 , floor.second.default_floorid );
            if ( floor.second.teleport_point.has_value() )
            {
                sqlite3_bind_int( this->sql_statement_handler , 5 , floor.second.teleport_point.value().x );
                sqlite3_bind_int( this->sql_statement_handler , 6 , floor.second.teleport_point.value().y );
            }
            else
            {
                sqlite3_bind_null( this->sql_statement_handler , 5 );
                sqlite3_bind_null( this->sql_statement_handler , 6 );
            }
            if ( floor.second.field_vision.has_value() )
            {
                sqlite3_bind_int( this->sql_statement_handler , 7 , floor.second.field_vision.value().x );
                sqlite3_bind_int( this->sql_statement_handler , 8 , floor.second.field_vision.value().y );
            }
            else
            {
                sqlite3_bind_null( this->sql_statement_handler , 7 );
                sqlite3_bind_null( this->sql_statement_handler , 8 );
            }
            sqlite3_bind_text( this->sql_statement_handler , 9 , floor.second.name.c_str() , floor.second.name.size() , SQLITE_STATIC );
            sqlite3_bind_blob( this->sql_statement_handler , 10 , floor.second.content.data() ,
                sizeof( MagicTower::TowerGrid )*floor.second.length*floor.second.width , SQLITE_STATIC );

            //UPDATE not return data so sqlite3_step not return SQLITE_ROW
            this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_DONE )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( "reset statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }

        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
    }

    void DataBase::set_script_flags( std::map<std::string , std::uint32_t>& flags )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , nullptr , nullptr , nullptr );

        //clear old flag,make sure flag_name all value unique
        sqlite3_exec( this->db_handler , "DELETE FROM script_flags" , nullptr , nullptr , nullptr );

        const char sql_statement[] = "INSERT OR REPLACE INTO script_flags(flag_name,flag_value) VALUES( ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 2 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "sql statement:\"" ) + std::string( sql_statement ) + std::string( "\" bind argument count out of expectation" ) );
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
                throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( "reset statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }
        
        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , nullptr , nullptr , nullptr );
        
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
    }

    void DataBase::set_inventories( std::map<std::uint32_t , std::uint32_t>& inventories )
    {
        sqlite3_exec( this->db_handler , "BEGIN TRANSACTION" , nullptr , nullptr , nullptr );

        const char sql_statement[] = "INSERT OR REPLACE INTO inventories(item_id,item_number) VALUES( ? , ? )";
        this->sqlite3_error_code = sqlite3_prepare_v2( this->db_handler , 
        sql_statement , sizeof( sql_statement ) , &( this->sql_statement_handler ) , nullptr );

        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "prepare statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
        
        if ( sqlite3_bind_parameter_count( this->sql_statement_handler ) != 2 )
        {
            sqlite3_finalize( this->sql_statement_handler );
            throw std::runtime_error( std::string( "sql statement:\"" ) + std::string( sql_statement ) + std::string( "\" bind argument count out of expectation" ) );
        }

        for( auto& item : inventories )
        {
            sqlite3_bind_int( this->sql_statement_handler , 1 , item.first );
            sqlite3_bind_int64( this->sql_statement_handler , 2 , item.second );

            //UPDATE or INSERT not return data so sqlite3_step not return SQLITE_ROW
            this->sqlite3_error_code = sqlite3_step( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_DONE )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( "evaluate statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
            }
            this->sqlite3_error_code = sqlite3_reset( this->sql_statement_handler );
            if ( this->sqlite3_error_code != SQLITE_OK )
            {
                sqlite3_finalize( this->sql_statement_handler );
                throw std::runtime_error( std::string( "reset statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
            }
            sqlite3_clear_bindings( this->sql_statement_handler );
        }
        
        sqlite3_exec( this->db_handler , "COMMIT TRANSACTION" , nullptr , nullptr , nullptr );
        
        this->sqlite3_error_code = sqlite3_finalize( this->sql_statement_handler );
        if ( this->sqlite3_error_code != SQLITE_OK )
        {
            throw std::runtime_error( std::string( "finalize statement:" ) + std::string( sql_statement ) + std::string( " failure,slite3 error code:" ) + std::to_string( this->sqlite3_error_code ) );
        }
    }
}
