#pragma once
#ifndef MUSIC_H
#define MUSIC_H

//in mingw64,std::random_device not working,do not use
//#include <random>

#include <cstdint>

#include <vector>
#include <exception>
#include <string>

namespace MagicTower
{
    class music_init_failure : public std::runtime_error
    {
    public:
        explicit music_init_failure( const std::string &s ) : std::runtime_error(s) {}
    };

    enum class PLAY_STATE:std::uint32_t
    {
        PLAYING = 0,
        PAUSE,
        STOP,
    };

    enum class PLAY_MODE:std::uint32_t
    {
        SINGLE_PLAY = 0,
        SINGLE_CYCLE,
        LIST_CYCLE,
        RANDOM_PLAYING,
    };

    struct MusicImp;

    class Music
    {
    public:
        Music( std::vector<std::string> _play_list = {} );
        ~Music();

        bool play( std::size_t id );
        bool play( std::string uri );
        bool next();
        void pause();
        void resume();

        PLAY_STATE get_state();
        double get_volume();
        std::vector<std::string> get_music_uri_list();

        void set_volume( double volume );
        void set_music_uri_list( std::vector<std::string> uri_list );
        void set_play_mode( PLAY_MODE mode );

        void add_music_uri_list( std::vector<std::string> uri_list );

        static bool is_music_file( std::string& file_uri );

        Music( const Music& rhs )=delete;
        Music( Music&& rhs )=delete;
        Music& operator=( const Music& rhs )=delete;
        Music& operator=( Music&& rhs )=delete;
    private:
        MusicImp * imp_ptr;
    };
}

#endif
