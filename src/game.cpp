#include <vector>

#include <glibmm.h>
#include <giomm.h>
#include <glib/gstdio.h>

#include "game_window.h"

static std::vector<Glib::ustring> log_content;

static GLogWriterOutput mt_log_writer( GLogLevelFlags log_level, const GLogField *fields , gsize n_fields , gpointer )
{
    log_content.push_back( Glib::ustring( g_log_writer_format_fields( log_level , fields , n_fields , false ) ) );

    return G_LOG_WRITER_HANDLED;
}

int main( int , char * argv[] )
{
    std::unique_ptr< char , decltype( &g_free ) > self_dir_path( g_path_get_dirname( argv[0] ) , g_free );
    g_chdir( self_dir_path.get() );
    g_log_set_writer_func( mt_log_writer , nullptr , nullptr );

    MagicTower::GameWindow game;
    game.run();

    Glib::RefPtr<Gio::File> logfile = Gio::File::create_for_path( "magictower.log" );
    if ( logfile->query_exists() )
    {
        logfile->remove();
    }
    Glib::RefPtr<Gio::FileIOStream> io_streamer = logfile->create_file_readwrite();
    Glib::RefPtr<Gio::OutputStream> output_streamer = io_streamer->get_output_stream();
    for ( auto& log : log_content )
    {
        output_streamer->write( log );
    }
    return EXIT_SUCCESS;
}
