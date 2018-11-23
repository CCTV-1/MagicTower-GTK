#pragma once
#ifndef MUSIC_H
#define MUSIC_H

//in mingw64,std::random_device not working,do not use
//#include <random>

#include <cstdint>

#include <vector>
#include <memory>

namespace MagicTower
{
    class music_init_failure : public std::runtime_error
    {
    public:
        explicit music_init_failure( const std::string &s ) : std::runtime_error(s) {}
    };

    enum PLAY_STATE:std::uint32_t
    {
        PLAYING = 0,
        PAUSE,
        STOP,
    };

    enum PLAY_MODE:std::uint32_t
    {
        SIGLE_CYCLE = 0,
        LIST_CYCLE,
        RANDOM_PLAYING,
    };

    struct MusicImp;

    class Music
    {
    public:
        Music( std::vector<std::shared_ptr<const char> > _play_list );
        ~Music();

        bool play( std::size_t id );
        bool play_next();
        void play_stop();
        void play_pause();
        void play_restart();
        void play_resume();

        enum PLAY_STATE get_state();
        double get_volume();
        std::vector<std::shared_ptr<const char> > get_music_uri_list();

        void set_volume( double volume );
        void set_music_uri_list( std::vector<std::shared_ptr<const char> > uri_list );
        void set_play_mode( PLAY_MODE mode );

        void add_music_uri_list( std::vector<std::shared_ptr<const char> > uri_list );

        Music( const Music& rhs )=delete;
        Music( Music&& rhs )=delete;
        Music& operator=( const Music& rhs )=delete;
        Music& operator=( Music&& rhs )=delete;
    private:
        MusicImp * imp_ptr;
    };
}

#endif
