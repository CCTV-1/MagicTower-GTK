#pragma once
#ifndef RESOURCES_H
#define RESOURCES_H

#include <string>
#include <vector>

class ResourcesManager
{
public:
    static std::string get_image( std::string image_type , std::uint32_t image_id );
    static std::string get_soundeffect_uri( std::string soundeffect_name );
    static std::vector<std::string> get_images( void );
    static std::vector<std::string> get_musics_uri( void );
    static std::string get_save_path( std::uint32_t save_id );
    static std::string get_script_path( void );
};

#endif
