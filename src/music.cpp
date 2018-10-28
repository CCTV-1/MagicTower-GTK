#include "music.h"

namespace MagicTower
{

static void have_type_handler( GstElement * typefind , guint probability , const GstCaps * caps , GstCaps ** p_caps );
static void pad_added_handler( GstElement * src , GstPad * new_pad , GstElement * conver );
static bool is_music( std::shared_ptr<const char>& file_uri );
static gboolean sigle_cycle( GstBus * bus , GstMessage * msg , GstElement * pipeline );
static gboolean list_cycle( GstBus * bus , GstMessage * msg , Music * music );
static gboolean random_playing( GstBus * bus , GstMessage * msg , Music * music );

Music::Music( std::vector<std::shared_ptr<const char> >& _music_uri_list , PLAY_MODE _mode , std::size_t _play_id )
    :music_uri_list( _music_uri_list ),mode( _mode ),play_id( _play_id )
{
    gboolean init_status = true;
    if ( gst_is_initialized() == false )
        init_status = gst_init_check( nullptr , nullptr , nullptr );
    if ( init_status == false )
        throw gst_init_failure( std::string( "gstreamer initial failure" ) );

    this->pipeline = gst_pipeline_new( "audio-player" );
    this->source = gst_element_factory_make( "uridecodebin" , "source" );
    this->conver = gst_element_factory_make( "audioconvert" , "conver" );
#if 0 //def G_OS_WIN32
    this->sink   = gst_element_factory_make( "directsoundsink" , "sink" );
#else
    this->sink   = gst_element_factory_make( "playsink" , "sink" );
#endif

    if( this->pipeline == nullptr || this->source == nullptr || this->conver == nullptr || this->sink == nullptr )
        throw gst_init_failure( std::string( "element could not be created" ) );

    gst_bin_add_many( GST_BIN( this->pipeline ) , this->source , this->conver , this->sink , nullptr );
    if ( gst_element_link( this->conver , this->sink ) != TRUE )
        throw gst_init_failure( std::string( "elements could not be linked" ) );

    this->grand_gen = g_rand_new();
    this->play_signal_id = 0;
    if ( play_id > this->music_uri_list.size() )
        this->play_id = 0;

    this->set_play_mode( this->mode );

    if ( this->music_uri_list.empty() )
        throw music_list_empty( std::string( "music uri list is empty" ) );
    //uri to file format check
    for ( auto uri_iterator = this->music_uri_list.begin() ; uri_iterator != this->music_uri_list.end() ; )
    {
        if ( is_music( *uri_iterator ) )
            uri_iterator++;
        else
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "\'%s\' not be music file,remove from play list." , g_filename_from_uri( uri_iterator->get() , nullptr , nullptr ) );
            this->music_uri_list.erase( uri_iterator );
        }
    }
    g_object_set( G_OBJECT( this->source ) , "uri" , this->music_uri_list[ play_id ].get() , nullptr );
    g_signal_connect( this->source , "pad-added" , G_CALLBACK( pad_added_handler ) , this->conver );
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        throw music_play_failure( std::string( "unable to set the pipeline to the playing state" ) );
    }
}

Music::~Music()
{
    gst_element_set_state( this->pipeline , GST_STATE_NULL );
    gst_object_unref( this->pipeline );
}

gboolean Music::play( std::size_t id )
{
    this->play_id = id;
    g_object_set( G_OBJECT( this->source ) , "uri" , this->music_uri_list[ id ].get() , nullptr );
    g_signal_connect( this->source , "pad-added" , G_CALLBACK( pad_added_handler ) , this->conver );
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return FALSE;
    }
    return TRUE;
}

gboolean Music::play_next()
{
    std::shared_ptr<GstBus> bus(
        gst_element_get_bus( this->pipeline ) , gst_object_unref
    );
    gst_bus_post( bus.get() , gst_message_new_eos( GST_OBJECT( bus.get() ) ) );
    return TRUE;
}

void Music::play_stop()
{
    gst_element_set_state( this->pipeline , GST_STATE_READY );
}

void Music::play_pause()
{
    gst_element_set_state( this->pipeline , GST_STATE_PAUSED );
}

void Music::play_resume()
{
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return ;
    }
}

void Music::play_restart()
{
    gst_element_set_state( this->pipeline , GST_STATE_READY );
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
        return ;
    }
}

enum PLAY_STATE Music::get_state()
{
    GstState state = GST_STATE_NULL;
    GstStateChangeReturn ret = gst_element_get_state( GST_ELEMENT( this->pipeline ) , &state , nullptr , -1 );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable get the playing state" );
        return PLAY_STATE::STOP;
    }

    switch ( state )
    {
        case GST_STATE_PAUSED:
            return PLAY_STATE::PAUSE;
        case GST_STATE_PLAYING:
            return PLAY_STATE::PLAYING;
        default:
            return PLAY_STATE::STOP;
    }
    return PLAY_STATE::STOP;
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
    this->mode = mode;
    std::shared_ptr<GstBus> bus(
        gst_element_get_bus( this->pipeline ) , gst_object_unref
    );
    if ( this->play_signal_id == 0 )
        gst_bus_add_signal_watch( bus.get() );
    else
        g_signal_handler_disconnect( G_OBJECT( bus.get() ) , this->play_signal_id );
    switch ( mode )
    {
        case PLAY_MODE::SIGLE_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
        case PLAY_MODE::LIST_CYCLE:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( list_cycle ) , this->pipeline );
            break;
        }
        case RANDOM_PLAYING:
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( random_playing ) , this );
            break;
        }
        default :
        {
            this->play_signal_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->pipeline );
            break;
        }
    }
}

static bool is_music( std::shared_ptr<const char>& file_uri )
{
    GstCaps * caps = nullptr;
    std::shared_ptr<gchar> filename(
        g_filename_from_uri( file_uri.get() , nullptr , nullptr ),
        []( char * music_file_name ){ g_free( music_file_name ); }
    );
    std::shared_ptr<GstElement> pipeline( gst_pipeline_new( "format_check" ) , gst_object_unref );
    GstElement * source = gst_element_factory_make( "filesrc" , "source" );
    GstElement * typefind = gst_element_factory_make( "typefind" , "typefind" );
    GstElement * sink   = gst_element_factory_make( "fakesink" , "fakesink" );
    gst_bin_add_many( GST_BIN( pipeline.get() ) , source , typefind , sink , nullptr );
    gst_element_link_many( source , typefind , sink , nullptr );
    g_signal_connect( G_OBJECT( typefind ) , "have-type" , G_CALLBACK( have_type_handler ) , &caps );
    g_object_set( source , "location" , filename.get() , nullptr );
    GstState state = GST_STATE_NULL;
    gst_element_set_state( GST_ELEMENT( pipeline.get() ) , GST_STATE_PAUSED );
    GstStateChangeReturn ret = gst_element_get_state( GST_ELEMENT( pipeline.get() ) , &state , nullptr , -1 );
    switch( ret )
    {
        case GST_STATE_CHANGE_FAILURE:
        {
            std::shared_ptr<GstBus> bus( gst_pipeline_get_bus( GST_PIPELINE( pipeline.get() ) ) , gst_object_unref );
            std::shared_ptr<GstMessage> msg( gst_bus_poll( bus.get() , GST_MESSAGE_ERROR , 0 ) , gst_message_unref );
            GError * err = nullptr;

            if ( msg != nullptr )
            {
                gst_message_parse_error( msg.get() , &err , nullptr );
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s - FAILED: %s" , filename.get() , err->message );
                g_clear_error( &err );
            }
            else
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s - FAILED: unknown error" , filename.get() );
            }
            gst_element_set_state( pipeline.get() , GST_STATE_NULL );
            return false;
        }
        case GST_STATE_CHANGE_SUCCESS:
        {
            #ifdef DEBUG
            if ( caps )
            {
                std::shared_ptr<gchar> caps_str( gst_caps_to_string( caps ) , g_free );
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "%s - %s" , filename.get() , caps_str.get() );
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
    
    gst_element_set_state( pipeline.get() , GST_STATE_NULL );
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
    std::shared_ptr<GstPad> sink_pad( gst_element_get_static_pad( conver , "sink" ) , gst_object_unref );

    /* if our converter is already linked, we have nothing to do here */
    if ( gst_pad_is_linked( sink_pad.get() ) )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "We are already linked. ignoring." );
        return ;
    }
    
    /* Check the new pad's type */
    std::shared_ptr<GstCaps> new_pad_caps( gst_pad_query_caps( new_pad , nullptr ) , gst_caps_unref );
    GstStructure * new_pad_struct = gst_caps_get_structure( new_pad_caps.get() , 0 );
    const gchar * new_pad_type = gst_structure_get_name( new_pad_struct );
    
    if ( !g_str_has_prefix( new_pad_type , "audio/x-raw" ) )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "It has type '%s' which is not raw audio. Ignoring." , new_pad_type );
        return ;
    }
    
    /* Attempt the link */
    GstPadLinkReturn ret = gst_pad_link( new_pad , sink_pad.get() );
    if ( GST_PAD_LINK_FAILED( ret ) )
    {
        g_log( __func__ , G_LOG_LEVEL_MESSAGE , "Type is '%s' but link failed." , new_pad_type );
    }
    
}

static gboolean sigle_cycle( GstBus * bus , GstMessage * msg , GstElement * pipeline )
{
    ( void )bus;
    ( void )msg;
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
    g_object_set( source , "uri" , music->get_music_uri_list()[ i ].get() , nullptr );
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
    g_object_set( source , "uri" , music->get_music_uri_list()[ i ].get() , nullptr );
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
