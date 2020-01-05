#pragma once
#ifndef DATABASE_H
#define DATABASE_H

#include <cstddef>
#include <string>
#include <map>

#include <sqlite3.h>

#include "hero.h"
#include "tower.h"

namespace MagicTower
{
    class DataBase
    {
    public:
        DataBase( std::string filename = std::string( "magictower.db" ) );
        ~DataBase();

        Hero get_hero_info( std::size_t archive_id );
        TowerMap get_tower_info( void );
        std::map<std::string , std::uint32_t> get_script_flags( void );
        std::map<std::uint32_t , std::uint32_t> get_inventories( void );

        void set_hero_info( const Hero& hero , std::size_t archive_id );
        void set_tower_info( const TowerMap& tower );
        void set_script_flags( std::map<std::string , std::uint32_t>& flags );
        void set_inventories( std::map<std::uint32_t , std::uint32_t>& inventories );

    protected:
        DataBase( const DataBase& )=delete;
        DataBase( DataBase&& )=delete;
        DataBase& operator=( const DataBase& )=delete;
        DataBase& operator=( const DataBase&& )=delete;
    private:
        void create_tables();
        std::string db_filename;
        int sqlite3_error_code;
        sqlite3 * db_handler;
        sqlite3_stmt * sql_statement_handler;
    };
}

#endif
