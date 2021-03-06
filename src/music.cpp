#include <memory>

#include "music.h"

#include <glib.h>
#include <gst/gst.h>

namespace MagicTower
{
    static void have_type_handler( GstElement * typefind , guint probability , const GstCaps * caps , GstCaps ** p_caps );
    static void pad_added_handler( GstElement * , GstPad * new_pad , GstElement * conver );
    static gboolean single_play( GstBus * , GstMessage * , GstElement * );
    static gboolean single_cycle( GstBus * bus , GstMessage * msg , GstElement * pipeline );
    static gboolean list_cycle( GstBus * bus , GstMessage * msg , MusicPlayerImp * imp_ptr );
    static gboolean random_playing( GstBus * bus , GstMessage * msg , MusicPlayerImp * imp_ptr );

    struct MusicPlayerImp
    {
        MusicPlayerImp():
            mode( PLAY_MODE::RANDOM_PLAYING ),
            state( PLAY_STATE::STOP ),
            play_id( 0 ),
            handler_id( 0 ),
            music_uri_list( {} ),
            grand_gen( g_rand_new() , g_rand_free )
        {
            gboolean init_status = true;
            if ( !gst_is_initialized() )
                init_status = gst_init_check( nullptr , nullptr , nullptr );
            if ( !init_status )
                throw std::runtime_error( std::string( "gstreamer initial failure" ) );
            if ( ( this->pipeline = gst_pipeline_new( "audio-player" ) ) == nullptr )
            {
                throw std::runtime_error( std::string( "gstreamer pipeline could not be created" ) );
            }
            if ( ( this->source = gst_element_factory_make( "uridecodebin" , "source" ) ) == nullptr )
            {
                gst_object_unref( this->pipeline );
                throw std::runtime_error( std::string( "gstreamer element uridecodebin could not be created" ) );
            }
            if ( ( this->conver = gst_element_factory_make( "audioconvert" , "conver" ) ) == nullptr )
            {
                gst_object_unref( this->pipeline );
                gst_object_unref( this->source );
                throw std::runtime_error( std::string( "gstreamer element audioconvert could not be created" ) );
            }
            if ( ( this->sink = gst_element_factory_make( "playsink" , "sink" ) ) == nullptr )
            {
                gst_object_unref( this->pipeline );
                gst_object_unref( this->source );
                gst_object_unref( this->conver );
                throw std::runtime_error( std::string( "gstreamer element playsink could not be created" ) );
            }
    
            gst_bin_add_many( GST_BIN( this->pipeline ) , this->source , this->conver , this->sink , nullptr );
            if ( gst_element_link( this->conver , this->sink ) != TRUE )
            {
                gst_object_unref( this->pipeline );
                gst_object_unref( this->source );
                gst_object_unref( this->conver );
                gst_object_unref( this->sink );
                throw std::runtime_error( std::string( "elements could not be linked" ) );
            }
            g_signal_connect( this->source , "pad-added" , G_CALLBACK( pad_added_handler ) , this->conver );
        }
        ~MusicPlayerImp()
        {
            gst_element_set_state( this->pipeline , GST_STATE_NULL );
            gst_object_unref( this->pipeline );
        }
        gint32 get_random_id()
        {
            return g_rand_int_range( this->grand_gen.get() , 0 , this->music_uri_list.size() );
        }

        PLAY_MODE mode;
        PLAY_STATE state;
        std::size_t play_id;
        gulong handler_id;
        std::vector<std::string> music_uri_list;
        GstElement * pipeline;
        GstElement * source;
        GstElement * conver;
        GstElement * sink;
        std::unique_ptr< GRand , decltype( &g_rand_free ) > grand_gen;
    };

    MusicPlayer::MusicPlayer( std::vector<std::string> _music_uri_list ):
        imp_ptr( new MusicPlayerImp )
    {
        if ( _music_uri_list.empty() )
            return ;
        set_playmode( this->imp_ptr->mode );
        add_playlist( _music_uri_list );
        play( this->imp_ptr->play_id );
    }

    MusicPlayer::~MusicPlayer()
    {
        delete this->imp_ptr;
    }

    bool MusicPlayer::play( std::size_t id )
    {
        if ( this->imp_ptr->music_uri_list.empty() )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "playlist has empty" );
            return false;
        }

        if ( this->imp_ptr->state == PLAY_STATE::PAUSE )
        {
            return false;
        }

        this->imp_ptr->play_id = id;
        g_object_set( G_OBJECT( this->imp_ptr->source ) , "uri" , this->imp_ptr->music_uri_list[ id ].c_str() , nullptr );
        GstStateChangeReturn ret = gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return false;
        }
        this->imp_ptr->state = PLAY_STATE::PLAYING;
        return true;
    }

    bool MusicPlayer::play( std::string uri )
    {
        if ( uri.empty() )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "uri not found" );
            return false;
        }

        if ( this->imp_ptr->state == PLAY_STATE::PAUSE )
        {
            return false;
        }

        gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_NULL );
        g_object_set( G_OBJECT( this->imp_ptr->source ) , "uri" , uri.c_str() , nullptr );
        GstStateChangeReturn ret = gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return false;
        }
        this->imp_ptr->state = PLAY_STATE::PLAYING;
        return true;
    }

    bool MusicPlayer::next()
    {
        if ( this->imp_ptr->state == PLAY_STATE::PAUSE )
        {
            return false;
        }
        std::unique_ptr< GstBus , decltype( &gst_object_unref ) > bus( gst_element_get_bus( this->imp_ptr->pipeline ) , gst_object_unref );
        gst_bus_post( bus.get() , gst_message_new_eos( GST_OBJECT( bus.get() ) ) );
        this->imp_ptr->state = PLAY_STATE::PLAYING;
        return true;
    }
    
    void MusicPlayer::pause()
    {
        GstState state = GST_STATE_NULL;
        GstStateChangeReturn ret = gst_element_get_state( GST_ELEMENT( this->imp_ptr->pipeline ) , &state , nullptr , -1 );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable get the playing state" );
            return ;
        }
        if ( state == GST_STATE_PLAYING )
        {
            gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PAUSED );
        }
        this->imp_ptr->state = PLAY_STATE::PAUSE;
    }

    void MusicPlayer::resume()
    {
        if ( this->imp_ptr->state != PLAY_STATE::PAUSE )
        {
            return ;
        }
        GstState state = GST_STATE_NULL;
        GstStateChangeReturn ret = gst_element_get_state( GST_ELEMENT( this->imp_ptr->pipeline ) , &state , nullptr , -1 );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable get the playing state" );
            return ;
        }
        if ( state == GST_STATE_PAUSED )
        {
            ret = gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PLAYING );
            if ( ret == GST_STATE_CHANGE_FAILURE )
            {
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
                return ;
            }
        }
        this->imp_ptr->state = PLAY_STATE::PLAYING;
    }

    PLAY_STATE MusicPlayer::get_state()
    {
        return this->imp_ptr->state;
    }

    void MusicPlayer::set_volume( double volume )
    {
        g_object_set( G_OBJECT( this->imp_ptr->sink ) , "volume" , volume , NULL );
    }

    double MusicPlayer::get_volume()
    {
        gdouble volume;
        g_object_get( G_OBJECT( this->imp_ptr->sink ) , "volume" , &volume , NULL );
        return volume;
    }

    void MusicPlayer::add_playlist( std::vector<std::string> uri_list )
    {
        for ( auto uri : uri_list )
        {
            if ( is_music_file( uri ) )
            {
                this->imp_ptr->music_uri_list.insert( this->imp_ptr->music_uri_list.end() , uri );
            }
            else
            {
                std::unique_ptr< gchar , decltype( &g_free ) > filename( g_filename_from_uri( uri.c_str() , nullptr , nullptr ) , g_free );
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "\'%s\' not be music file,remove from play list." , filename.get() );
                continue;
            }
        }
    }
    
    void MusicPlayer::set_playlist( std::vector<std::string> uri_list )
    {
        this->imp_ptr->music_uri_list.clear();
        add_playlist( std::move( uri_list ) );
    }

    std::vector<std::string> MusicPlayer::get_playlist()
    {
        return this->imp_ptr->music_uri_list;
    }

    void MusicPlayer::set_playmode( PLAY_MODE mode )
    {
        this->imp_ptr->mode = mode;
        std::unique_ptr< GstBus , decltype( &gst_object_unref ) > bus( gst_element_get_bus( this->imp_ptr->pipeline ) , gst_object_unref );
        if ( this->imp_ptr->handler_id == 0 )
            gst_bus_add_signal_watch( bus.get() );
        else
            g_signal_handler_disconnect( G_OBJECT( bus.get() ) , this->imp_ptr->handler_id );
        switch ( mode )
        {
            case PLAY_MODE::SINGLE_CYCLE:
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( single_cycle ) , this->imp_ptr->pipeline );
                break;
            }
            case PLAY_MODE::LIST_CYCLE:
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( list_cycle ) , this->imp_ptr );
                break;
            }
            case PLAY_MODE::RANDOM_PLAYING:
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( random_playing ) , this->imp_ptr );
                break;
            }
            default:
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( single_play ) , this->imp_ptr->pipeline );
                break;
            }
        }
    }

    bool MusicPlayer::is_music_file( std::string& file_uri )
    {
        GstCaps * caps = nullptr;
        std::unique_ptr< gchar , decltype( &g_free ) > filename( g_filename_from_uri( file_uri.c_str() , nullptr , nullptr ) , g_free );
        std::unique_ptr< GstElement , decltype( &gst_object_unref ) > pipeline( gst_pipeline_new( "format_check" ) , gst_object_unref );
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
                std::unique_ptr< GstBus , decltype( &gst_object_unref ) > bus( gst_pipeline_get_bus( GST_PIPELINE( pipeline.get() ) ) , gst_object_unref );
                std::unique_ptr< GstMessage , decltype( &gst_message_unref ) > msg( gst_bus_poll( bus.get() , GST_MESSAGE_ERROR , 0 ) , gst_message_unref );
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
                    std::unique_ptr< gchar , decltype( &g_free ) > caps_str( gst_caps_to_string( caps ) , g_free );
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

    static void have_type_handler( GstElement * , guint , const GstCaps * caps , GstCaps ** p_caps )
    {
        if ( p_caps )
        {
            *p_caps = gst_caps_copy( caps );
        }
    }

    static void pad_added_handler( GstElement * , GstPad * new_pad , GstElement * conver )
    {
        std::unique_ptr< GstPad , decltype( &gst_object_unref ) > sink_pad( gst_element_get_static_pad( conver , "sink" ) , gst_object_unref );

        /* if our converter is already linked, we have nothing to do here */
        if ( gst_pad_is_linked( sink_pad.get() ) )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "We are already linked. ignoring." );
            return ;
        }

        /* Check the new pad's type */
        std::unique_ptr< GstCaps , decltype( &gst_caps_unref ) > new_pad_caps( gst_pad_query_caps( new_pad , nullptr ) , gst_caps_unref );
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

    static gboolean single_play( GstBus * , GstMessage * , GstElement * pipeline )
    {
        gst_element_set_state( pipeline , GST_STATE_NULL );
        return TRUE;
    }

    static gboolean single_cycle( GstBus * , GstMessage * , GstElement * pipeline )
    {
        gst_element_set_state( pipeline , GST_STATE_NULL );
        GstStateChangeReturn ret = gst_element_set_state( pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return FALSE;
        }
        return TRUE;
    }

    static gboolean list_cycle( GstBus * , GstMessage * , MusicPlayerImp * imp_ptr )
    {
        GstElement * pipeline = imp_ptr->pipeline;
        GstElement * source = imp_ptr->source;
        gst_element_unlink( imp_ptr->conver , imp_ptr->sink );
        gst_element_set_state( pipeline , GST_STATE_NULL );

        std::size_t i = imp_ptr->play_id;
        if ( i <= imp_ptr->music_uri_list.size() )
            i = 0;
        else
            i = i + 1;
        imp_ptr->play_id = i;
        g_object_set( source , "uri" , imp_ptr->music_uri_list[ i ].c_str() , nullptr );
        gst_element_link( imp_ptr->conver , imp_ptr->sink );
        GstStateChangeReturn ret = gst_element_set_state( pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return FALSE;
        }
        return TRUE;
    }

    static gboolean random_playing( GstBus * , GstMessage * , MusicPlayerImp * imp_ptr )
    {
        GstElement * pipeline = imp_ptr->pipeline;
        GstElement * source = imp_ptr->source;
        gst_element_unlink( imp_ptr->conver , imp_ptr->sink );
        gst_element_set_state( pipeline , GST_STATE_NULL );

        std::size_t i = imp_ptr->get_random_id();
        imp_ptr->play_id = i;
        g_object_set( source , "uri" , imp_ptr->music_uri_list[ i ].c_str() , nullptr );
        gst_element_link( imp_ptr->conver , imp_ptr->sink );
        GstStateChangeReturn ret = gst_element_set_state( pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return FALSE;
        }
        return TRUE;
    }

}
