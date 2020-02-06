#include <string>
#include <vector>

#include <glibmm.h>
#include <giomm.h>

#include "resources.h"

#define IMAGE_RESOURCES_PATH "./resources/images/"
#define MUSIC_RESOURCES_PATH "./resources/music/"
#define DATABSE_RESOURCES_PATH "./resources/database/"
#define CUSTOM_SCRIPTS_PATH "./resources/scripts/"

std::string ResourcesManager::get_image( std::string image_type , std::uint32_t image_id )
{
    return std::string( IMAGE_RESOURCES_PATH ) + image_type + std::to_string( image_id ) + std::string( ".png" );
}

std::vector<std::string> ResourcesManager::get_images( void )
{
    //exists and type check
    if ( !Glib::file_test( IMAGE_RESOURCES_PATH , Glib::FileTest::FILE_TEST_EXISTS ) )
        return {};
    if ( !Glib::file_test( IMAGE_RESOURCES_PATH , Glib::FileTest::FILE_TEST_IS_DIR ) )
        return {};

    Glib::Dir image_dir( IMAGE_RESOURCES_PATH );
    std::vector<std::string> images;
    std::string name;
    while( ( name = image_dir.read_name() ) != std::string( "" ) )
    {
        images.push_back(IMAGE_RESOURCES_PATH + name);
    }
    return images;
}

std::vector<std::string> ResourcesManager::get_musics_uri( void )
{
    if ( !Glib::file_test( MUSIC_RESOURCES_PATH , Glib::FileTest::FILE_TEST_EXISTS ) )
    {
        return {};
    }
    if ( !Glib::file_test( MUSIC_RESOURCES_PATH , Glib::FileTest::FILE_TEST_IS_DIR ) )
    {
        return {};
    }

    Glib::Dir music_dir( MUSIC_RESOURCES_PATH );
    std::vector<std::string> uri_list;
    std::string name;
    while( ( name = music_dir.read_name() ) != std::string( "" ) )
    {
        Glib::RefPtr<Gio::File> music_file = Gio::File::create_for_path( MUSIC_RESOURCES_PATH + name );
        uri_list.push_back( music_file->get_uri() );
    }
    return uri_list;
}

std::string ResourcesManager::get_save_path( std::uint32_t save_id )
{
    return std::string( DATABSE_RESOURCES_PATH ) + std::to_string( save_id ) + std::string( ".db" );
}

std::string ResourcesManager::get_script_path( void )
{
    return std::string(CUSTOM_SCRIPTS_PATH);
}