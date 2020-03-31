#pragma once
#ifndef MUSIC_H
#define MUSIC_H

#include <cstdint>

#include <vector>
#include <exception>
#include <string>

namespace MagicTower
{
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

    struct MusicPlayerImp;

    class MusicPlayer
    {
    public:
        MusicPlayer( std::vector<std::string> _play_list = {} );
        ~MusicPlayer();

        bool play( std::size_t id );
        bool play( std::string uri );
        bool next();
        void pause();
        void resume();

        PLAY_STATE get_state();
        double get_volume();
        std::vector<std::string> get_playlist();

        void set_volume( double volume );
        void set_playlist( std::vector<std::string> uri_list );
        void set_playmode( PLAY_MODE mode );

        void add_playlist( std::vector<std::string> uri_list );

        static bool is_music_file( std::string& file_uri );

        MusicPlayer( const MusicPlayer& rhs )=delete;
        MusicPlayer( MusicPlayer&& rhs )=delete;
        MusicPlayer& operator=( const MusicPlayer& rhs )=delete;
        MusicPlayer& operator=( MusicPlayer&& rhs )=delete;
    private:
        MusicPlayerImp * imp_ptr;
    };
}

#endif
