#include "music.h"

#include <glib.h>
#include <gst/gst.h>

namespace MagicTower
{
    static void pad_added_handler( GstElement * src , GstPad * new_pad , GstElement * conver );
    struct MusicImp
    {
        MusicImp():
            mode( PLAY_MODE::RANDOM_PLAYING ),
            play_id( 0 ),
            handler_id( 0 ),
            music_uri_list( {} ),
            grand_gen( g_rand_new() , g_rand_free )
        {
            gboolean init_status = true;
            if ( gst_is_initialized() == FALSE )
                init_status = gst_init_check( nullptr , nullptr , nullptr );
            if ( init_status == false )
                throw music_init_failure( std::string( "gstreamer initial failure" ) );
            if ( ( this->pipeline = gst_pipeline_new( "audio-player" ) ) == nullptr )
            {
                throw music_init_failure( std::string( "gstreamer pipeline could not be created" ) );
            }
            if ( ( this->source = gst_element_factory_make( "uridecodebin" , "source" ) ) == nullptr )
            {
                gst_object_unref( this->pipeline );
                throw music_init_failure( std::string( "gstreamer element uridecodebin could not be created" ) );
            }
            if ( ( this->conver = gst_element_factory_make( "audioconvert" , "conver" ) ) == nullptr )
            {
                gst_object_unref( this->pipeline );
                gst_object_unref( this->source );
                throw music_init_failure( std::string( "gstreamer element audioconvert could not be created" ) );
            }
            if ( ( this->sink = gst_element_factory_make( "playsink" , "sink" ) ) == nullptr )
            {
                gst_object_unref( this->pipeline );
                gst_object_unref( this->source );
                gst_object_unref( this->conver );
                throw music_init_failure( std::string( "gstreamer element playsink could not be created" ) );
            }
    
            gst_bin_add_many( GST_BIN( this->pipeline ) , this->source , this->conver , this->sink , nullptr );
            if ( gst_element_link( this->conver , this->sink ) != TRUE )
            {
                gst_object_unref( this->pipeline );
                gst_object_unref( this->source );
                gst_object_unref( this->conver );
                gst_object_unref( this->sink );
                throw music_init_failure( std::string( "elements could not be linked" ) );
            }
            g_signal_connect( this->source , "pad-added" , G_CALLBACK( pad_added_handler ) , this->conver );
        }
        ~MusicImp()
        {
            gst_element_set_state( this->pipeline , GST_STATE_NULL );
            gst_object_unref( this->pipeline );
        }
        gint32 get_random_id()
        {
            return g_rand_int_range( this->grand_gen.get() , 0 , this->music_uri_list.size() );
        }

        PLAY_MODE mode;
        std::size_t play_id;
        gulong handler_id;
        std::vector<std::shared_ptr<const char> > music_uri_list;
        GstElement * pipeline;
        GstElement * source;
        GstElement * conver;
        GstElement * sink;
        std::shared_ptr<GRand> grand_gen;
    };

    static void have_type_handler( GstElement * typefind , guint probability , const GstCaps * caps , GstCaps ** p_caps );
    static bool is_music( std::shared_ptr<const char>& file_uri );
    static gboolean sigle_cycle( GstBus * bus , GstMessage * msg , GstElement * pipeline );
    static gboolean list_cycle( GstBus * bus , GstMessage * msg , MusicImp * imp_ptr );
    static gboolean random_playing( GstBus * bus , GstMessage * msg , MusicImp * imp_ptr );

    Music::Music():
        imp_ptr( new MusicImp )
    {
        ;
    }

    Music::Music( std::vector<std::shared_ptr<const char> > _music_uri_list ):
        imp_ptr( new MusicImp )
    {
        set_play_mode( this->imp_ptr->mode );
        add_music_uri_list( _music_uri_list );
        play( this->imp_ptr->play_id );
    }

    Music::~Music()
    {
        delete this->imp_ptr;
    }

    bool Music::play( std::size_t id )
    {
        this->imp_ptr->play_id = id;
        g_object_set( G_OBJECT( this->imp_ptr->source ) , "uri" , this->imp_ptr->music_uri_list[ id ].get() , nullptr );
        GstStateChangeReturn ret = gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return FALSE;
        }
        return TRUE;
    }

    bool Music::play_next()
    {
        std::shared_ptr<GstBus> bus( gst_element_get_bus( this->imp_ptr->pipeline ) , gst_object_unref );
        gst_bus_post( bus.get() , gst_message_new_eos( GST_OBJECT( bus.get() ) ) );
        return TRUE;
    }

    void Music::play_stop()
    {
        gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_READY );
    }
    
    void Music::play_pause()
    {
        gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PAUSED );
    }

    void Music::play_resume()
    {
        GstStateChangeReturn ret = gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return ;
        }
    }

    void Music::play_restart()
    {
        gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_READY );
        GstStateChangeReturn ret = gst_element_set_state( this->imp_ptr->pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return ;
        }
    }

    enum PLAY_STATE Music::get_state()
    {
        GstState state = GST_STATE_NULL;
        GstStateChangeReturn ret = gst_element_get_state( GST_ELEMENT( this->imp_ptr->pipeline ) , &state , nullptr , -1 );
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

    void Music::set_volume( double volume )
    {
        g_object_set( G_OBJECT( this->imp_ptr->sink ) , "volume" , volume , NULL );
    }

    double Music::get_volume()
    {
        gdouble volume;
        g_object_get( G_OBJECT( this->imp_ptr->sink ) , "volume" , &volume , NULL );
        return volume;
    }

    void Music::add_music_uri_list( std::vector<std::shared_ptr<const char> > uri_list )
    {
        for ( auto uri : uri_list )
        {
            if ( is_music( uri ) == true )
            {
                this->imp_ptr->music_uri_list.insert( this->imp_ptr->music_uri_list.end() , uri );
            }
            else
            {
                std::shared_ptr<gchar> filename( g_filename_from_uri( uri.get() , nullptr , nullptr ) , g_free );
                g_log( __func__ , G_LOG_LEVEL_MESSAGE , "\'%s\' not be music file,remove from play list." , filename.get() );
                continue;
            }
        }
    }
    
    void Music::set_music_uri_list( std::vector<std::shared_ptr<const char> > uri_list )
    {
        this->imp_ptr->music_uri_list.clear();
        add_music_uri_list( std::move( uri_list ) );
    }

    std::vector<std::shared_ptr<const char> > Music::get_music_uri_list()
    {
        return this->imp_ptr->music_uri_list;
    }

    void Music::set_play_mode( PLAY_MODE mode )
    {
        this->imp_ptr->mode = mode;
        std::shared_ptr<GstBus> bus( gst_element_get_bus( this->imp_ptr->pipeline ) , gst_object_unref );
        if ( this->imp_ptr->handler_id == 0 )
            gst_bus_add_signal_watch( bus.get() );
        else
            g_signal_handler_disconnect( G_OBJECT( bus.get() ) , this->imp_ptr->handler_id );
        switch ( mode )
        {
            case PLAY_MODE::SIGLE_CYCLE:
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->imp_ptr->pipeline );
                break;
            }
            case PLAY_MODE::LIST_CYCLE:
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( list_cycle ) , this->imp_ptr );
                break;
            }
            case RANDOM_PLAYING:
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( random_playing ) , this->imp_ptr );
                break;
            }
            default :
            {
                this->imp_ptr->handler_id = g_signal_connect( G_OBJECT( bus.get() ) , "message::eos" , G_CALLBACK( sigle_cycle ) , this->imp_ptr->pipeline );
                break;
            }
        }
    }

    static bool is_music( std::shared_ptr<const char>& file_uri )
    {
        GstCaps * caps = nullptr;
        std::shared_ptr<gchar> filename( g_filename_from_uri( file_uri.get() , nullptr , nullptr ) , g_free );
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

    static gboolean list_cycle( GstBus * bus , GstMessage * msg , MusicImp * imp_ptr )
    {
        ( void )bus;
        ( void )msg;
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
        g_object_set( source , "uri" , imp_ptr->music_uri_list[ i ].get() , nullptr );
        gst_element_link( imp_ptr->conver , imp_ptr->sink );
        GstStateChangeReturn ret = gst_element_set_state( pipeline , GST_STATE_PLAYING );
        if ( ret == GST_STATE_CHANGE_FAILURE )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "unable to set the pipeline to the playing state" );
            return FALSE;
        }
        return TRUE;
    }

    static gboolean random_playing( GstBus * bus , GstMessage * msg , MusicImp * imp_ptr )
    {
        ( void )bus;
        ( void )msg;
        GstElement * pipeline = imp_ptr->pipeline;
        GstElement * source = imp_ptr->source;
        gst_element_unlink( imp_ptr->conver , imp_ptr->sink );
        gst_element_set_state( pipeline , GST_STATE_NULL );

        std::size_t i = imp_ptr->get_random_id();
        imp_ptr->play_id = i;
        g_object_set( source , "uri" , imp_ptr->music_uri_list[ i ].get() , nullptr );
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
