#pragma once
#ifndef DATABASE_H
#define DATABASE_H

#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include <sqlite3.h>

#include "hero.h"
#include "item.h"
#include "monster.h"
#include "store.h"
#include "stairs.h"
#include "tower.h"

namespace MagicTower
{

class sqlite_open_failure : public std::runtime_error
{
public:
    explicit sqlite_open_failure( const int error_code , const std::string &s ) : std::runtime_error(s)
    {
        std::string code_str = std::to_string( error_code );
        this->s = std::string( "open file:" ) + s + std::string( " failure,slite3 error code:" ) + code_str;
    }
    const char * what() { return s.c_str(); };
private:
    std::string s;
};

class sqlite_prepare_statement_failure : public std::runtime_error
{
public:
    explicit sqlite_prepare_statement_failure( const int error_code , const std::string &s ) : std::runtime_error(s)
    {
        std::string code_str = std::to_string( error_code );
        this->s = std::string( "prepare statement:" ) + s + std::string( " failure,slite3 error code:" ) + code_str;
    }
    const char * what() { return s.c_str(); };
private:
    std::string s;
};

class sqlite_bind_int_failure : public std::runtime_error
{
public:
    explicit sqlite_bind_int_failure( const int arg_pos , const int arg_val , const int error_code , const std::string &s ) : std::runtime_error(s)
    {
        std::string argument_pos_str = std::to_string( arg_pos );
        std::string argument_val_str = std::to_string( arg_val );
        std::string code_str = std::to_string( error_code );
        this->s = std::string( "bind statement:\"" ) + s + std::string( "\" failure,bind argument postion:" )\
                                + argument_pos_str + std::string( ",bind argument value:" )\
                                + argument_val_str + std::string( ",sqlite error code:" ) + code_str;
    }
    const char * what() { return s.c_str(); };
private:
    std::string s;
};

class sqlite_bind_count_failure : public std::runtime_error
{
public:
    explicit sqlite_bind_count_failure( const std::string& s ) : std::runtime_error(s)
    {
        this->s = std::string( "sql statement:\"" ) + s + std::string( "\" bind argument count out of expectation" );
    }
    const char * what() { return s.c_str(); };
private:
    std::string s;
};

class sqlite_evaluate_statement_failure : public std::runtime_error
{
public:
    explicit sqlite_evaluate_statement_failure( const int error_code , const std::string &s ) : std::runtime_error(s)
    {
        std::string code_str = std::to_string( error_code );
        this->s = std::string( "evaluate statement:" ) + s + std::string( " failure,slite3 error code:" ) + code_str;
    }
    const char * what() { return s.c_str(); };
private:
    std::string s;
};

class sqlite_reset_statement_failure : public std::runtime_error
{
public:
    explicit sqlite_reset_statement_failure( const int error_code , const std::string &s ) : std::runtime_error(s)
    {
        std::string code_str = std::to_string( error_code );
        this->s = std::string( "reset statement:" ) + s + std::string( " failure,slite3 error code:" ) + code_str;
    }
    const char * what() { return s.c_str(); };
private:
    std::string s;
};

class sqlite_table_format_failure : public std::runtime_error
{
public:
    explicit sqlite_table_format_failure( const std::string &s ) : std::runtime_error(s) { }
};

class sqlite_finalize_statement_failure : public std::runtime_error
{
public:
    explicit sqlite_finalize_statement_failure( const int error_code , const std::string &s ) : std::runtime_error(s)
    {
        std::string code_str = std::to_string( error_code );
        this->s = std::string( "finalize statement:" ) + s + std::string( " failure,slite3 error code:" ) + code_str;
    }
    const char * what() { return s.c_str(); };
private:
    std::string s;
};

class DataBase
{
public:
    DataBase();
    DataBase( const char * filename );
    ~DataBase();

    Hero get_hero_info( std::size_t archive_id );
    Tower get_tower_info( std::size_t archive_id );

    std::map<event_position_t , std::shared_ptr<char> > get_custom_events();
    std::vector<Stairs> get_stairs_list();
    std::vector<Store> get_store_list();
    std::vector<Item> get_item_list();
    std::vector<Monster> get_monster_list();

    void set_hero_info( const Hero& hero , std::size_t archive_id );
    void set_tower_info( const Tower& tower , std::size_t archive_id );

    void set_item_list( const std::vector<Item>& items );
    void set_monster_list( const std::vector<Monster>& monsters );
    void set_stairs_list( const std::vector<Stairs>& stairs );

protected:
    DataBase( const DataBase& )=delete;
    DataBase( DataBase&& )=delete;
    DataBase& operator=( const DataBase& )=delete;
    DataBase& operator=( const DataBase&& )=delete;
private:
    std::string db_filename;
    int sqlite3_error_code;
    sqlite3 * db_handler;
    sqlite3_stmt * sql_statement_handler;
};

}

#endif
