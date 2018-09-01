#include "music.h"

namespace MagicTower
{

static void pad_added_handler( GstElement * src , GstPad * new_pad , GstElement * conver );
static gboolean sigle_cycle( GstBus * bus , GstMessage * msg , GstElement * pipeline );
static gboolean list_cycle( GstBus * bus , GstMessage * msg , Music * music );
static gboolean random_playing( GstBus * bus , GstMessage * msg , Music * music );


Music::Music( int * argc , char ** argv[] , std::vector<std::shared_ptr<const char> >& music_uri_list )
    :music_uri_list( music_uri_list )
{
    if ( this->music_uri_list.empty() )
        throw music_list_empty( std::string( "music uri list is empty" ) );
    gst_init( argc , argv );
    this->pipeline = gst_pipeline_new( "audio-player" );
    this->source = gst_element_factory_make( "uridecodebin" , "source" );
    this->conver = gst_element_factory_make( "audioconvert" , "conver" );
    this->sink   = gst_element_factory_make( "directsoundsink" , "sink"  );
    this->play_id = 0;
    this->mode = PLAY_MODE::SIGLE_CYCLE;
    if( this->pipeline == NULL || this->source == NULL || this->conver == NULL || this->sink == NULL )
        throw gst_element_make_failure( std::string( "element could not be created\n" ) );

    gst_bin_add_many( GST_BIN( this->pipeline ) , this->source , this->conver , this->sink , NULL );
    if ( gst_element_link( this->conver , this->sink ) != TRUE )
        throw gst_element_link_failure( std::string( "elements could not be linked\n" ) );

    this->grand_gen = g_rand_new();
}

Music::~Music()
{
    gst_element_set_state( this->pipeline , GST_STATE_NULL );
    gst_object_unref( this->pipeline );
}

gboolean Music::play_start( PLAY_MODE mode )
{
    this->mode = mode;
    gint32 i = this->get_random_id();
    this->set_play_id( i );
    g_object_set( G_OBJECT( this->source ) , "uri" , this->music_uri_list[ i ].get() , NULL );
    g_signal_connect( this->source , "pad-added" , G_CALLBACK( pad_added_handler ) , this->conver );
    GstStateChangeReturn ret = gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
    if ( ret == GST_STATE_CHANGE_FAILURE )
    {
        g_printerr( "unable to set the this->pipeline to the playing state\n" );
        return FALSE;
    }

    GstBus * bus = gst_element_get_bus( this->pipeline );
    gst_bus_add_signal_watch( bus );
    switch ( mode )
    {
        case PLAY_MODE::SIGLE_CYCLE:
        {
            g_signal_connect( G_OBJECT( bus ) , "message::eos" , ( GCallback )sigle_cycle , this->pipeline );
            break;
        }
        case PLAY_MODE::LIST_CYCLE:
        {
            g_signal_connect( G_OBJECT( bus ) , "message::eos" , ( GCallback )list_cycle , this->pipeline );
            break;
        }
        case RANDOM_PLAYING:
        {
            g_signal_connect( G_OBJECT( bus ) , "message::eos" , ( GCallback )random_playing , this );
            break;
        }
        default :
        {
            g_signal_connect( G_OBJECT( bus ) , "message::eos" , ( GCallback )sigle_cycle , this->pipeline );
            break;
        }
    }

    gst_object_unref( bus );
    return TRUE;
}

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
    this->set_play_id( i );
    gst_element_unlink( this->conver , this->sink );
    gst_element_set_state( this->pipeline , GST_STATE_NULL );
    g_object_set( G_OBJECT( this->source ) , "uri" , this->music_uri_list[ i ].get() , NULL );
    gst_element_link( this->conver , this->sink );
    gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
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
    gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
}

void Music::play_restart()
{
    gst_element_set_state( this->pipeline , GST_STATE_READY );
    gst_element_set_state( this->pipeline , GST_STATE_PLAYING );
}

std::size_t Music::get_play_id()
{
    return this->play_id;
}

void Music::set_play_id( std::size_t play_id )
{
    this->play_id = play_id;
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

static void pad_added_handler( GstElement * src , GstPad * new_pad , GstElement * conver )
{
    ( void )src;
    GstPad * sink_pad = gst_element_get_static_pad( conver , "sink" );
    GstPadLinkReturn ret;
    GstCaps * new_pad_caps = NULL;
    GstStructure * new_pad_struct = NULL;
    const gchar * new_pad_type = NULL;
    
    /* If our converter is already linked, we have nothing to do here */
    if ( gst_pad_is_linked( sink_pad ) )
    {
        g_print( "We are already linked. Ignoring.\n" );
        goto exit;
    }
    
    /* Check the new pad's type */
    new_pad_caps = gst_pad_query_caps( new_pad, NULL );
    new_pad_struct = gst_caps_get_structure( new_pad_caps , 0 );
    new_pad_type = gst_structure_get_name( new_pad_struct );
    if ( !g_str_has_prefix( new_pad_type , "audio/x-raw" ) )
    {
        g_print( "It has type '%s' which is not raw audio. Ignoring.\n" , new_pad_type );
        goto exit;
    }
    
    /* Attempt the link */
    ret = gst_pad_link( new_pad , sink_pad );
    if ( GST_PAD_LINK_FAILED( ret ) )
    {
        g_print( "Type is '%s' but link failed.\n" , new_pad_type );
    }
    
    exit:
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
    gst_element_set_state( pipeline , GST_STATE_PLAYING );
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
    gst_element_set_state( pipeline , GST_STATE_PLAYING );
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
    gst_element_set_state( pipeline , GST_STATE_PLAYING );
    return TRUE;
}

}
