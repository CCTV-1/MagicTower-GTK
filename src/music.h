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

class gst_element_make_failure : public std::runtime_error
{
public:
    explicit gst_element_make_failure( const std::string &s ) : std::runtime_error(s) {}
};

class gst_element_link_failure : public std::runtime_error
{
public:
    explicit gst_element_link_failure( const std::string &s ) : std::runtime_error(s) {}
};

class music_list_empty : public std::runtime_error
{
public:
    explicit music_list_empty( const std::string &s ) : std::runtime_error(s) {}
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
    Music( int * argc , char ** argv[] , std::vector<std::shared_ptr<const char> >& play_list );
    ~Music();
    gboolean play_start( PLAY_MODE mode = PLAY_MODE::RANDOM_PLAYING );
    gboolean play_next();
    void play_stop();
    void play_pause();
    void play_restart();
    void play_resume();
    std::size_t get_play_id();
    void set_play_id( std::size_t play_id );
    GstElement * get_pipeline();
    GstElement * get_source();
    GstElement * get_conver();
    GstElement * get_sink();
    gint32 get_random_id();
    std::vector<std::shared_ptr<const char> >& get_music_uri_list();
protected:
    Music( const Music& )=delete;
    Music( Music&& )=delete;
    Music& operator=( const Music& )=delete;
    Music& operator=( const Music&& )=delete;
private:
    std::size_t play_id;
    PLAY_MODE mode;
    GstElement * pipeline;
    GstElement * source;
    GstElement * conver;
    GstElement * sink;
    std::vector<std::shared_ptr<const char> >& music_uri_list;
    GRand * grand_gen;
};

}

#endif
