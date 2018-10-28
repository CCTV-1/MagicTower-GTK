#pragma once
#ifndef MUSIC_H
#define MUSIC_H

//in mingw64,std::random_device not working,do not use
//#include <random>

#include <cstdint>

#include <vector>
#include <memory>

#include <glib.h>
#include <gst/gst.h>

namespace MagicTower
{
    class gst_init_failure : public std::runtime_error
    {
    public:
        explicit gst_init_failure( const std::string &s ) : std::runtime_error(s) {}
    };
    class music_list_empty : public std::runtime_error
    {
    public:
        explicit music_list_empty( const std::string &s ) : std::runtime_error(s) {}
    };

    class music_play_failure : public std::runtime_error
    {
    public:
        explicit music_play_failure( const std::string &s ) : std::runtime_error(s) {}
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

    class Music
    {
    public:
        Music( std::vector<std::shared_ptr<const char> >& _play_list , 
            PLAY_MODE _mode = PLAY_MODE::RANDOM_PLAYING , std::size_t _play_id = 0 );
        ~Music();
        
        gboolean play( std::size_t id );
        gboolean play_next();
        void play_stop();
        void play_pause();
        void play_restart();
        void play_resume();
        enum PLAY_STATE get_state();
        std::size_t get_play_id();
        GstElement * get_pipeline();
        GstElement * get_source();
        GstElement * get_conver();
        GstElement * get_sink();
        gint32 get_random_id();
        std::vector<std::shared_ptr<const char> >& get_music_uri_list();

        void set_play_id( std::size_t play_id );
        void set_play_mode( PLAY_MODE mode );

        Music( const Music& rhs )=delete;
        Music( Music&& rhs )=delete;
        Music& operator=( const Music& rhs )=delete;
        Music& operator=( const Music&& rhs )=delete;
    private:
        std::vector<std::shared_ptr<const char> >& music_uri_list;
        PLAY_MODE mode;
        std::size_t play_id;
        GstElement * pipeline;
        GstElement * source;
        GstElement * conver;
        GstElement * sink;
        gulong play_signal_id;
        GRand * grand_gen;
    };
}

#endif
