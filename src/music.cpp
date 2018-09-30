#include "music.h"

namespace MagicTower
{

static void have_type_handler( GstElement * typefind , guint probability , const GstCaps * caps , GstCaps ** p_caps );
static void pad_added_handler( GstElement * src , GstPad * new_pad , GstElement * conver );
static bool file_format_check( std::shared_ptr<const char>& file_uri );
static gboolean sigle_cycle( GstBus * bus , GstMessage * msg , GstElement * pipeline );
static gboolean list_cycle( GstBus * bus , GstMessage * msg , Music * music );
static gboolean random_playing( GstBus * bus , GstMessage * msg , Music * music );

Music::Music( int * argc , char ** argv[] , std::vector<std::shared_ptr<const char> >& _music_uri_list , PLAY_MODE _mode , PLAY_STATE _state , std::size_t _play_id )
    :music_uri_list( _music_uri_list ),mode( _mode ),state( _state ),play_id( _play_id )
{
    if ( this->music_uri_list.empty() )
        throw music_list_empty( std::string( "music uri list is empty" ) );
    gst_init( argc , argv );

    //uri to file format check
    for ( auto music_uri_iterator = this->music_uri_list.begin() ; music_uri_iterator != this->music_uri_list.end() ; )
    {
        bool is_music = file_format_check( *music_uri_iterator );
        if ( is_music )
            music_uri_iterator++;
        else
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "\'%s\' not be music file,remove from play list." , g_filename_from_uri( ( *music_uri_iterator ).get() , NULL , NULL ) );
            this->music_uri_list.erase( music_uri_iterator );
        }
    }

    this->pipeline = gst_pipeline_new( "audio-player" );
    this->source = gst_element_factory_make( "uridecodebin" , "source" );
    this->conver = gst_element_factory_make( "audioconvert" , "conver" );
#if 0 //def G_OS_WIN32
    this->sink   = gst_element_factory_make( "directsoundsink" , "sink" );
#else
    this->sink   = gst_element_factory_make( "playsink" , "sink" );
#endif

    if ( play_id > this->music_uri_list.size() )
        this->play_id = 0;
    if( this->pipeline == NULL || this->source == NULL || this->conver == NULL || this->sink == NULL )
        throw gst_element_make_failure( std::string( "element could not be created\n" ) );

    gst_bin_add_many( GST_BIN( this->pipeline ) , this->source , this->conver , this->sink , NULL );
    if ( gst_element_link( this->conver , this->sink ) != TRUE )
        throw gst_element_link_failure( std::string( "elements could not be linked\n" ) );

    this->grand_gen = g_rand_new();

    g_object_set( G_OBJECT( this->source ) , "uri" , this->music_uri_list[ play_id ].get() , NULL );
    g_signal_connect( this->source , "pad-added" , G_CALLBACK( pad_added_handler ) , this->conver );

    GstBus * bus = gst_element_get_bus( this->pipeline );
    gst_bus_add_signal_watch( bus );
    switch ( this->mode )
    {
        case PLAY_MODE::SIGLE_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
        case PLAY_MODE::LIST_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( list_cycle ) , this->pipeline );
            break;
        }
        case RANDOM_PLAYING:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( random_playing ) , this );
            break;
        }
        default :
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
    }
    gst_object_unref( bus );

    if ( this->state != PLAY_STATE::PLAYING )
        return ;
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        throw music_play_failure( std::string( "unable to set the pipeline to the playing state\n" ) );
    }
}

Music::~Music()
{
    gst_element_set_state( this->pipeline , GST_STATE_NULL );
    gst_object_unref( this->pipeline );
}

/* gboolean Music::play_start( PLAY_MODE mode )
{
    this->mode = mode;
    gint32 i = this->get_random_id();
    this->set_play_id( i );
    g_object_set( G_OBJECT( this->source ) , "uri" , this->music_uri_list[ i ].get() , NULL );
    g_signal_connect( this->source , "pad-added" , G_CALLBACK( pad_added_handler ) , this->conver );
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the this->pipeline to the playing state" );
        return FALSE;
    }

    GstBus * bus = gst_element_get_bus( this->pipeline );
    gst_bus_add_signal_watch( bus );
    switch ( mode )
    {
        case PLAY_MODE::SIGLE_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
        case PLAY_MODE::LIST_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( list_cycle ) , this->pipeline );
            break;
        }
        case RANDOM_PLAYING:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( random_playing ) , this );
            break;
        }
        default :
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
    }

    gst_object_unref( bus );
    return TRUE;
} */

gboolean Music::play_next()
{
    std::size_t i;
    switch( this->mode )
    {
        case PLAY_MODE::SIGLE_CYCLE:
            return TRUE;
        case PLAY_MODE::LIST_CYCLE:
        {
            i = this->play_id;
            if ( i <= this->music_uri_list.size() )
                i = 0;
            else
                i = i + 1;
            break;
        }
        case PLAY_MODE::RANDOM_PLAYING:
        {
            i = this->get_random_id();
            break;
        }
        default :
            return TRUE;
    }
    this->play_id = i;
    gst_element_unlink( this->conver , this->sink );
    gst_element_set_state( this->pipeline , GST_STATE_NULL );
    g_object_set( G_OBJECT( this->source ) , "uri" , this->music_uri_list[ i ].get() , NULL );
    gst_element_link( this->conver , this->sink );
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return FALSE;
    }
    return TRUE;
}

void Music::play_stop()
{
    if ( this->state == PLAY_STATE::STOP )
        return ;
    gst_element_set_state( this->pipeline , GST_STATE_READY );
    this->state = PLAY_STATE::STOP;
}

void Music::play_pause()
{
    if ( this->state == PLAY_STATE::PAUSE )
        return ;
    gst_element_set_state( this->pipeline , GST_STATE_PAUSED );
    this->state = PLAY_STATE::PAUSE;
}

void Music::play_resume()
{
    if ( this->state != PLAY_STATE::PAUSE )
        return ;
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return ;
    }
    this->state = PLAY_STATE::PLAYING;
}

void Music::play_restart()
{
    if ( this->state != PLAY_STATE::STOP )
        return ;
    gst_element_set_state( this->pipeline , GST_STATE_READY );
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return ;
    }
    this->state = PLAY_STATE::PLAYING;
}

enum PLAY_STATE Music::get_state()
{
    return this->state;
}

std::size_t Music::get_play_id()
{
    return this->play_id;
}

GstElement * Music::get_pipeline()
{
    return this->pipeline;
}

GstElement * Music::get_source()
{
    return this->source;
}

GstElement * Music::get_conver()
{
    return this->conver;
}

GstElement * Music::get_sink()
{
    return this->sink;
}

std::vector<std::shared_ptr<const char> >& Music::get_music_uri_list()
{
    return this->music_uri_list;
}

gint32 Music::get_random_id()
{
    gint32 id = g_rand_int_range( this->grand_gen , 0 , this->music_uri_list.size() );
    return id;
}

void Music::set_play_id( std::size_t play_id )
{
    this->play_id = play_id;
}

void Music::set_play_mode( PLAY_MODE mode )
{
    if ( mode == this->mode )
        return ;
    this->mode = mode;
    GstBus * bus = gst_element_get_bus( this->pipeline );
    g_signal_handler_disconnect( G_OBJECT( bus ) , this->play_signal_id );
    switch ( mode )
    {
        case PLAY_MODE::SIGLE_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
        case PLAY_MODE::LIST_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( list_cycle ) , this->pipeline );
            break;
        }
        case RANDOM_PLAYING:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( random_playing ) , this );
            break;
        }
        default :
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
    }
    gst_object_unref( bus );
}

static bool file_format_check( std::shared_ptr<const char>& file_uri )
{
    GstCaps * caps = NULL;
    std::shared_ptr<gchar> filename(
        g_filename_from_uri( file_uri.get() , NULL , NULL ),
        []( char * music_file_name ){ g_free( music_file_name ); }
    );
    GstElement * pipeline = gst_pipeline_new( "format_check" );
    GstElement * source = gst_element_factory_make( "filesrc" , "source" );
    GstElement * typefind = gst_element_factory_make( "typefind" , "typefind" );
    GstElement * sink   = gst_element_factory_make( "fakesink" , "fakesink" );
    gst_bin_add_many( GST_BIN( pipeline ) , source , typefind , sink , NULL );
    gst_element_link_many( source , typefind, sink , NULL );
    g_signal_connect( G_OBJECT( typefind ) , "have-type" , G_CALLBACK( have_type_handler ) , &caps );
    g_object_set(source , "location" , filename.get() , NULL );
    GstState state = GST_STATE_NULL;
    gst_element_set_state( GST_ELEMENT( pipeline ) , GST_STATE_PAUSED );
    GstStateChangeReturn ret = gst_element_get_state( GST_ELEMENT( pipeline ) , &state , NULL , -1 );
    switch( ret )
    {
        case GST_STATE_CHANGE_FAILURE:
        {
            GstMessage * msg;
            GstBus * bus;
            GError * err = NULL;

            bus = gst_pipeline_get_bus( GST_PIPELINE( pipeline ) );
            msg = gst_bus_poll( bus , GST_MESSAGE_ERROR , 0 );
            gst_object_unref( bus );

            if ( msg )
            {
                gst_message_parse_error( msg , &err , NULL);
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s - FAILED: %s\n", filename.get() , err->message );
                g_clear_error( &err );
                gst_message_unref( msg );
            }
            else
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s - FAILED: unknown error", filename.get() );
            }
            gst_element_set_state( pipeline , GST_STATE_NULL );
            gst_object_unref( pipeline );
            return false;
        }
        case GST_STATE_CHANGE_SUCCESS:
        {
            #ifdef DEBUG
            if ( caps )
            {
                gchar * caps_str;

                caps_str = gst_caps_to_string( caps );
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s - %s" , filename.get() , caps_str );
                g_free( caps_str );
                gst_caps_unref( caps );
            }
            else
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s - %s" , filename.get() , "No type found" );
            }
            #endif
            break;
        }
        default:
            g_assert_not_reached();
    }
    
    gst_element_set_state( pipeline , GST_STATE_NULL );
    gst_object_unref( pipeline );
    return true;
}

static void have_type_handler( GstElement * typefind , guint probability , const GstCaps * caps , GstCaps ** p_caps )
{
    ( void )probability;
    ( void )typefind;
    if ( p_caps )
    {
        *p_caps = gst_caps_copy( caps );
    }
}

static void pad_added_handler( GstElement * src , GstPad * new_pad , GstElement * conver )
{
    ( void )src;
    GstPad * sink_pad = gst_element_get_static_pad( conver , "sink" );
    GstPadLinkReturn ret;
    GstCaps * new_pad_caps = NULL;
    GstStructure * new_pad_struct = NULL;
    const gchar * new_pad_type = NULL;

    /* if our converter is already linked, we have nothing to do here */
    if ( gst_pad_is_linked( sink_pad ) )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "We are already linked. ignoring." );
        goto handler_exit;
    }
    
    /* Check the new pad's type */
    new_pad_caps = gst_pad_query_caps( new_pad, NULL );
    new_pad_struct = gst_caps_get_structure( new_pad_caps , 0 );
    new_pad_type = gst_structure_get_name( new_pad_struct );
    if ( !g_str_has_prefix( new_pad_type , "audio/x-raw" ) )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "It has type '%s' which is not raw audio. Ignoring." , new_pad_type );
        goto handler_exit;
    }
    
    /* Attempt the link */
    ret = gst_pad_link( new_pad , sink_pad );
    if ( GST_PAD_LINK_FAILED( ret ) )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "Type is '%s' but link failed." , new_pad_type );
    }
    
    handler_exit:
    /* Unreference the new pad's caps, if we got them */
    if ( new_pad_caps != NULL )
        gst_caps_unref( new_pad_caps );
    
    /* Unreference the this->sink pad */
    gst_object_unref( sink_pad );
}

static gboolean sigle_cycle( GstBus * bus , GstMessage * msg , GstElement * pipeline )
{
    ( void )bus;
    ( void )msg;
    /*if ( gst_element_seek( pipeline , 1.0 , GST_FORMAT_TIME , GST_SEEK_FLAG_FLUSH ,
        GST_SEEK_TYPE_SET , 0*GST_SECOND , GST_SEEK_TYPE_NONE ,
        GST_CLOCK_TIME_NONE ) != TRUE )
    {
         g_printerr( "sigle_cycle failure\n" );
    } */
    gst_element_set_state( pipeline , GST_STATE_NULL );
    GstStateChangeReturn ret = gst_element_set_state( pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return FALSE;
    }
    return TRUE;
}

static gboolean list_cycle( GstBus * bus , GstMessage * msg , Music * music )
{
    ( void )bus;
    ( void )msg;
    GstElement * pipeline = music->get_pipeline();
    GstElement * source = music->get_source();
    gst_element_unlink( music->get_conver() , music->get_sink() );
    gst_element_set_state( pipeline , GST_STATE_NULL );

    std::size_t i = music->get_play_id();
    if ( i <= music->get_music_uri_list().size() )
        i = 0;
    else
        i = i + 1;
    music->set_play_id( i );
    g_object_set( source , "uri" , music->get_music_uri_list()[ i ].get() , NULL );
    gst_element_link( music->get_conver() , music->get_sink() );
    GstStateChangeReturn ret = gst_element_set_state( pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return FALSE;
    }
    return TRUE;
}

static gboolean random_playing( GstBus * bus , GstMessage * msg , Music * music )
{
    ( void )bus;
    ( void )msg;
    GstElement * pipeline = music->get_pipeline();
    GstElement * source = music->get_source();
    gst_element_unlink( music->get_conver() , music->get_sink() );
    gst_element_set_state( pipeline , GST_STATE_NULL );

    std::size_t i = music->get_random_id();
    music->set_play_id( i );
    g_object_set( source , "uri" , music->get_music_uri_list()[ i ].get() , NULL );
    gst_element_link( music->get_conver() , music->get_sink() );
    GstStateChangeReturn ret = gst_element_set_state( pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return FALSE;
    }
    return TRUE;
}

}
