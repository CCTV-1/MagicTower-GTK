#include <cstdlib>
#include <cstring>
#include <cinttypes>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <lua.hpp>

#include <cairomm/cairomm.h>
#include <gdkmm.h>
#include <gdk/gdkkeysyms.h>
#include <glibmm.h>
#include <glib/gstdio.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/main.h>
#include <gtkmm/widget.h>
#include <gtkmm/window.h>
#include <gtkmm/spinbutton.h>
#include <pangomm.h>
#include <sigc++/sigc++.h>

#include "env_var.h"
#include "game_event.h"
#include "game_window.h"

#define UI_DEFINE_RESOURCES_PATH "../resources/UI/magictower.ui"
#define IMAGE_RESOURCES_PATH "../resources/images/"
#define MUSIC_RESOURCES_PATH "../resources/music/"
#define CUSTOM_SCRIPTS_PATH "../resources/scripts/"

namespace MagicTower
{
    class GameWindowImp
    {
    public:
        GameWindowImp():
            game_object( new GameEnvironment() ),
            script_engines( luaL_newstate() , lua_close ),
            main_loop(),
            font_desc( "Microsoft YaHei 16" )
        {
            luaL_openlibs( this->script_engines.get() );
            //game environment to lua vm
            lua_pushlightuserdata( this->script_engines.get() , ( void * )this->game_object );
            //base64 game_object -> Z2FtZV9vYmplY3QK
            lua_setglobal( this->script_engines.get() , "Z2FtZV9vYmplY3QK" );

            this->scriptengines_register_eventfunc();

            std::vector<std::shared_ptr<const char> > music_list = this->load_music( MUSIC_RESOURCES_PATH );
            this->game_object->music.set_play_mode( PLAY_MODE::RANDOM_PLAYING );
            this->game_object->music.set_music_uri_list( music_list );
            this->game_object->music.play( 0 );

            Glib::RefPtr<Gdk::DisplayManager> display_manager = Gdk::DisplayManager::get();
            Glib::RefPtr<Gdk::Display> default_display = display_manager->get_default_display();
            Glib::RefPtr<const Gdk::Monitor> monitor = default_display->get_monitor( 0 );
            Gdk::Rectangle rectangle;
            monitor->get_workarea( rectangle );
            int grid_width = rectangle.get_width()/this->game_object->towers.LENGTH/3*2;
            int grid_height = rectangle.get_height()/this->game_object->towers.WIDTH;
            if ( grid_width > grid_height )
                grid_width = grid_height;
            this->pixel_size = grid_width/32*32;

            this->builder_refptr = Gtk::Builder::create_from_file( UI_DEFINE_RESOURCES_PATH );
            Gtk::SpinButton * layer_spin_button = nullptr;
            this->builder_refptr->get_widget( "layer_spin_button" , layer_spin_button );
            Glib::RefPtr<Gtk::Adjustment> layer_adjustment = Gtk::Adjustment::create( 1 , 1 , this->game_object->towers.HEIGHT , 1 , 10 , 0 );
            layer_spin_button->set_adjustment( layer_adjustment );
            Gtk::SpinButton * x_spin_button = nullptr;
            this->builder_refptr->get_widget( "x_spin_button" , x_spin_button );
            Glib::RefPtr<Gtk::Adjustment> x_adjustment = Gtk::Adjustment::create( 0 , 0 , this->game_object->towers.LENGTH , 1 , 1 , 0 );
            x_spin_button->set_adjustment( x_adjustment );
            Gtk::SpinButton * y_spin_button = nullptr;
            this->builder_refptr->get_widget( "y_spin_button" , y_spin_button );
            Glib::RefPtr<Gtk::Adjustment> y_adjustment = Gtk::Adjustment::create( 0 , 0 , this->game_object->towers.WIDTH , 1 , 1 , 0 );
            y_spin_button->set_adjustment( y_adjustment );

            int tower_width = ( this->game_object->towers.LENGTH )*this->pixel_size;
            int info_width = ( this->game_object->towers.LENGTH/2 )*this->pixel_size;
            int window_height = ( this->game_object->towers.LENGTH )*this->pixel_size;

            this->builder_refptr->get_widget( "info_area" , this->info_area );
            this->info_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_info ) );
            this->info_area->set_size_request( info_width , window_height );

            this->builder_refptr->get_widget( "tower_area" , this->game_area );
            this->game_area->add_events( Gdk::EventMask::BUTTON_PRESS_MASK );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_maps ) );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_path_line ) );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_hero ) );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_damage ) );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_tips ) );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_detail ) );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_menu ) );
            this->game_area->signal_draw().connect( sigc::mem_fun( *this , &GameWindowImp::draw_message ) );
            this->game_area->signal_button_press_event().connect( sigc::mem_fun( *this , &GameWindowImp::button_press_handler ) );
            this->game_area->set_size_request( tower_width , window_height );

            this->builder_refptr->get_widget( "game_window" , this->window );
            this->window->add_events( Gdk::EventMask::SCROLL_MASK );
            this->window->signal_delete_event().connect( sigc::mem_fun( *this , &GameWindowImp::exit_game ) );
            //if after is true,Key Up Space Down Return Left Right.... key_press_handler can't receive.
            this->window->signal_key_press_event().connect( sigc::mem_fun( *this , &GameWindowImp::key_press_handler ) , false );
            this->window->signal_scroll_event().connect( sigc::mem_fun( *this , &GameWindowImp::scroll_signal_handler ) );
            this->window->set_size_request( tower_width + info_width , window_height );
            this->window->show_all();

            this->layout = window->create_pango_layout( "字符串" );
            this->layout->set_font_description( this->font_desc );

            Glib::signal_timeout().connect( sigc::mem_fun( *this , &GameWindowImp::draw_loop ) , 100 );
            Glib::signal_timeout().connect( sigc::mem_fun( *this , &GameWindowImp::automatic_movement ) , 100 );

            this->info_frame = info_background_image_factory( this->game_object->towers.WIDTH/2 , this->game_object->towers.HEIGHT );
            this->image_resource = load_image_resource( IMAGE_RESOURCES_PATH );
        }

        ~GameWindowImp()
        {
            delete this->game_object;
            delete this->window;
        }

        void run()
        {
            this->main_loop.run( std::ref( *this->window ) );
        }

    protected:
        Glib::RefPtr<Gdk::Pixbuf> info_background_image_factory( size_t width , size_t height )
        {
            Glib::RefPtr<Gdk::Pixbuf> info_frame = Gdk::Pixbuf::create( Gdk::Colorspace::COLORSPACE_RGB , TRUE , 8 , width*this->pixel_size , height*this->pixel_size );

            Glib::RefPtr<Gdk::Pixbuf> bg_pixbuf = Gdk::Pixbuf::create_from_file( IMAGE_RESOURCES_PATH"floor11.png" , this->pixel_size , this->pixel_size );
            if( bg_pixbuf.get() == nullptr )
            {
                g_log( __func__ , G_LOG_LEVEL_WARNING , "\'%s\' not be image file" , IMAGE_RESOURCES_PATH"floor11.png" );
                return info_frame;
            }

            for ( size_t y = 0 ; y < height ; y++ )
            {
                for ( size_t x = 0 ; x < width ; x++ )
                {
                    bg_pixbuf->composite( info_frame , x*this->pixel_size , y*this->pixel_size , this->pixel_size , this->pixel_size ,
                    x*this->pixel_size , y*this->pixel_size , 1.0 , 1.0 , Gdk::InterpType::INTERP_NEAREST , 255 );
                }
            }

            return info_frame;
        }

        std::vector<std::shared_ptr<const char>> load_music( const char * music_path )
        {
            std::unique_ptr< GDir , decltype( &g_dir_close ) > dir_ptr( g_dir_open( music_path , 0 , nullptr ) , g_dir_close );

            if ( g_file_test( music_path , G_FILE_TEST_EXISTS ) != TRUE )
                return {};
            if ( g_file_test( music_path , G_FILE_TEST_IS_DIR ) != TRUE )
                return {};

            std::vector<std::shared_ptr<const char>> uri_array;

            //g_dir_read_name auto ignore "." ".."
            const gchar * filename;
            while ( ( filename = g_dir_read_name( dir_ptr.get() ) ) != nullptr )
            {
                std::unique_ptr< char , decltype( &g_free ) > music_file_name( g_strdup_printf( "%s/%s" , music_path , filename ) , g_free );
                std::unique_ptr< GFile , decltype( &g_object_unref ) > gfile_obj( g_file_new_for_path( music_file_name.get() ) , g_object_unref );
                std::shared_ptr<const char> file_uri( g_file_get_uri( gfile_obj.get() ) , g_free );
                uri_array.push_back( file_uri );
            }

            return uri_array;
        }

        std::map<std::string,Glib::RefPtr<Gdk::Pixbuf>> load_image_resource( const char * image_path )
        {
            std::unique_ptr< GDir , decltype( &g_dir_close ) > dir_ptr( g_dir_open( image_path , 0 , nullptr ) , g_dir_close );
            std::map<std::string,Glib::RefPtr<Gdk::Pixbuf>> image_resource;

            //exists and type check
            if ( g_file_test( image_path , G_FILE_TEST_EXISTS ) != TRUE )
                return {};
            if ( g_file_test( image_path , G_FILE_TEST_IS_DIR ) != TRUE )
                return {};

            //g_dir_read_name auto ignore "." ".."
            const gchar * filename;
            while ( ( filename = g_dir_read_name( dir_ptr.get() ) ) != nullptr )
            {
                std::string image_file_name = std::string( IMAGE_RESOURCES_PATH ) + std::string( filename );
                Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create_from_file( image_file_name , this->pixel_size , this->pixel_size );
                if ( pixbuf.get()  == nullptr )
                {
                    g_log( __func__ , G_LOG_LEVEL_MESSAGE , "\'%s\' not be image file,ignore this." , image_file_name.c_str() );
                    continue;
                }
                image_resource.insert({ image_file_name , pixbuf });
            }
            return image_resource;
        }

        Gdk::Rectangle get_menu_ractangle( void )
        {
            Gtk::Allocation allocation = this->game_area->get_allocation();
            const int widget_width = allocation.get_width();
            const int widget_height = allocation.get_height();
            const int box_start_x = widget_height/6;
            const int box_start_y = widget_width/6;

            const int box_height = widget_height*2/3;
            const int box_width = widget_width*2/3;
            return { box_start_x , box_start_y , box_width , box_height };
        }

        bool draw_loop( void )
        {
            if ( this->game_object->game_status == GAME_STATUS::GAME_END )
            {
                this->main_loop.quit();
            }
            this->info_area->queue_draw();
            this->game_area->queue_draw();
            return true;
        }

        bool automatic_movement( void )
        {
            auto game_object = this->game_object;
            if ( game_object->game_status != GAME_STATUS::FIND_PATH )
            {
                return true;
            }
            if ( game_object->path.empty() )
            {
                game_object->game_status = GAME_STATUS::NORMAL;
                return true;
            }
            TowerGridLocation goal = game_object->path.back();
            TowerGridLocation temp = { ( game_object->hero ).x , ( game_object->hero ).y };
            ( game_object->hero ).x = goal.x;
            ( game_object->hero ).y = goal.y;
            bool flags = trigger_collision_event( game_object , this->script_engines.get() );
            if ( flags == false )
            {
                ( game_object->hero ).x = temp.x;
                ( game_object->hero ).y = temp.y;
                //if goal is npc,game status set to dialog,now can't set game status to NORMAL
                if ( game_object->game_status == GAME_STATUS::FIND_PATH )
                    game_object->game_status = GAME_STATUS::NORMAL;
                return true;
            }
            game_object->path.pop_back();
            return true;
        }

        //GDK coordinate origin : Top left corner,left -> right x add,up -> down y add.
        void draw_grid_image( const Cairo::RefPtr<Cairo::Context> & cairo_context , std::uint32_t x , std::uint32_t y , std::string image_type , std::uint32_t image_id )
        {
            std::string image_file_name = std::string( IMAGE_RESOURCES_PATH ) + image_type + std::to_string( image_id ) + std::string( ".png" );
            const Glib::RefPtr<Gdk::Pixbuf> element = this->image_resource[ image_file_name ];
            if ( element.get() == nullptr )
            {
                g_log( __func__ , G_LOG_LEVEL_WARNING , "\'%s\' not be image file" , image_file_name.c_str() );
                return ;
            }
            Gdk::Cairo::set_source_pixbuf( cairo_context , element , x*this->pixel_size , y*this->pixel_size );
            cairo_context->paint();
        }

        //always return false to do other draw signal handler
        bool draw_maps( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            GameEnvironment * game_object = this->game_object;
            for( size_t y = 0 ; y < game_object->towers.LENGTH ; y++ )
            {
                for ( size_t x = 0 ; x < game_object->towers.WIDTH ; x++ )
                {
                    auto grid = get_tower_grid( game_object->towers , game_object->hero.layers , x , y );
                    switch( grid.type )
                    {
                        case GRID_TYPE::FLOOR:
                        {
                            this->draw_grid_image( cairo_context , x , y , "floor" , grid.id );
                            break;
                        }
                        case GRID_TYPE::WALL:
                        {
                            this->draw_grid_image( cairo_context , x , y , "wall" , grid.id );
                            break;
                        }
                        case GRID_TYPE::STAIRS:
                        {
                            this->draw_grid_image( cairo_context , x , y , "floor" , 1 );
                            this->draw_grid_image( cairo_context , x , y , "stairs" , game_object->stairs[ grid.id - 1 ].type );
                            break;
                        }
                        case GRID_TYPE::DOOR:
                        {
                            this->draw_grid_image( cairo_context , x , y , "floor" , 1 );
                            this->draw_grid_image( cairo_context , x , y , "door" , grid.id );
                            break;
                        }
                        case GRID_TYPE::NPC:
                        {
                            this->draw_grid_image( cairo_context , x , y , "floor" , 1 );
                            this->draw_grid_image( cairo_context , x , y , "npc" , grid.id );
                            break;
                        }
                        case GRID_TYPE::MONSTER:
                        {
                            this->draw_grid_image( cairo_context , x , y , "floor" , 1 );
                            this->draw_grid_image( cairo_context , x , y , "monster" , grid.id );
                            break;
                        }
                        case GRID_TYPE::ITEM:
                        {
                            this->draw_grid_image( cairo_context , x , y , "floor" , 1 );
                            this->draw_grid_image( cairo_context , x , y , "item" , grid.id );
                            break;
                        }
                        default :
                        {
                            this->draw_grid_image( cairo_context , x , y , "boundary" , 1 );
                            break;
                        }
                    }
                }
            }

            return false;
        }

        //always return false to do other draw signal handler
        bool draw_path_line( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            GameEnvironment * game_object = this->game_object;
            if ( game_object->game_status != GAME_STATUS::FIND_PATH )
                return false;
            if ( game_object->path.empty() )
                return false;
            if ( game_object->draw_path == false )
                return false;

            cairo_context->save();
            cairo_context->set_source_rgba( 1.0 , 0.2 , 0.2 , 1.0 );
            cairo_context->set_line_width( 4.0 );
            cairo_context->arc( ( ( game_object->path[0] ).x + 0.5 )*this->pixel_size , ( ( game_object->path[0] ).y + 0.5 )*this->pixel_size 
                , 0.1*this->pixel_size , 0 , 2*G_PI );
            cairo_context->fill();
            for ( auto point : game_object->path )
            {
                cairo_context->line_to( ( ( point ).x + 0.5 )*this->pixel_size , ( ( point ).y + 0.5 )*this->pixel_size );
                cairo_context->move_to( ( ( point ).x + 0.5 )*this->pixel_size , ( ( point ).y + 0.5 )*this->pixel_size );
            }
            cairo_context->line_to( ( ( game_object->hero ).x + 0.5 )*this->pixel_size , ( ( game_object->hero ).y + 0.5 )*this->pixel_size );
            cairo_context->stroke();
            cairo_context->restore();

            return false;
        }

        //always return false to do other draw signal handler
        bool draw_hero( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            draw_grid_image( cairo_context , ( game_object->hero ).x , ( game_object->hero ).y , "hero" , 1 );

            return false;
        }

        //always return false to do other draw signal handler
        bool draw_damage( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            GameEnvironment * game_object = this->game_object;
            for( size_t y = 0 ; y < game_object->towers.LENGTH ; y++ )
            {
                for ( size_t x = 0 ; x < game_object->towers.WIDTH ; x++ )
                {
                    auto grid = get_tower_grid( game_object->towers , game_object->hero.layers , x , y );
                    if ( grid.type == GRID_TYPE::MONSTER )
                    {
                        //todo:cache damage list
                        std::int64_t damage = get_combat_damage( game_object , grid.id );
                        std::string damage_text;
                        if ( damage >= 0 )
                            damage_text = std::to_string( damage );
                        else
                            damage_text = std::string( "????" );
                        this->layout->set_text( damage_text );

                        cairo_context->save();
                        cairo_context->move_to( x*this->pixel_size , ( y + 0.5 )*this->pixel_size );
                        double red_value = 0;
                        double green_value = 0;
                        if ( damage >= game_object->hero.life || damage < 0 )
                        {
                            red_value = 1;
                        }
                        else
                        {
                            red_value = static_cast<double>( damage )/( game_object->hero.life );
                            green_value = 1 - red_value;
                        }
                        cairo_context->set_source_rgb( red_value , green_value , 0.0 );
                        cairo_context->set_line_width( 0.5 );
                        this->layout->show_in_cairo_context( cairo_context );
                        cairo_context->fill_preserve();

                        cairo_context->stroke();
                        cairo_context->restore();
                    }
                }
            }

            return false;
        }

        //always return false to do other draw signal handler
        bool draw_detail( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            GameEnvironment * game_object = this->game_object;
            if ( game_object->game_status != GAME_STATUS::REVIEW_DETAIL )
                return false;

            std::int32_t x , y;
            std::string detail_str;
            x = this->click_x/this->pixel_size;
            y = this->click_y/this->pixel_size;
            auto grid = get_tower_grid( game_object->towers , game_object->hero.layers , x , y );
            if ( grid.type == GRID_TYPE::MONSTER )
            {
                auto monster = game_object->monsters[ grid.id - 1 ];
                detail_str = dump_monster_info( monster );
            }
            else if( grid.type == GRID_TYPE::ITEM )
            {
                auto item = game_object->items[ grid.id - 1 ];
                detail_str = dump_item_info( item );
            }
            else
            {
                game_object->game_status = GAME_STATUS::NORMAL;
                return false;
            }
            this->draw_dialog( cairo_context , detail_str , click_x , click_y );

            return false;
        }

        //always return false to do other draw signal handler
        bool draw_tips( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            GameEnvironment * game_object = this->game_object;
            if ( game_object->tips_content.empty() )
                return false;

            std::size_t tips_size = game_object->tips_content.size();

            cairo_context->save();
            for ( std::size_t i = 0 ; i < tips_size ; i++ )
            {
                this->layout->set_text( game_object->tips_content[i] );
                int layout_width , layout_height;
                this->layout->get_pixel_size( layout_width , layout_height );
                cairo_context->set_source_rgba( 43.0/255 , 42.0/255 , 43.0/255 , 0.7 );
                cairo_context->set_line_width( 2 );
                cairo_context->rectangle( 0 , 0 + i*layout_height , layout_width , layout_height );
                cairo_context->fill_preserve();
                cairo_context->set_source_rgba( 0 , 0 , 0 , 1.0 );
                cairo_context->stroke();

                cairo_context->move_to( 0 ,  0 + i*layout_height );
                cairo_context->set_line_width( 0.5 );
                cairo_context->set_source_rgb( 1.0 , 1.0 , 1.0 );
                this->layout->show_in_cairo_context( cairo_context );
                cairo_context->fill_preserve();
                cairo_context->stroke();
            }
            cairo_context->restore();

            return false;
        }

        //always return false to do other draw signal handler
        bool draw_menu( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            GameEnvironment * game_object = this->game_object;
            Gdk::Rectangle menu_rectangle = this->get_menu_ractangle();
            switch ( game_object->game_status )
            {
                case GAME_STATUS::START_MENU:
                case GAME_STATUS::STORE_MENU:
                case GAME_STATUS::GAME_MENU:
                case GAME_STATUS::JUMP_MENU:
                    break;
                default:
                    return false;
            }

            const int box_start_x = menu_rectangle.get_x();
            const int box_start_y = menu_rectangle.get_y();

            double box_width = menu_rectangle.get_width();
            double box_height = menu_rectangle.get_height();

            cairo_context->save();
            //draw box
            cairo_context->set_line_width( 2 );
            cairo_context->set_source_rgba( 43.0/255 , 42.0/255 , 43.0/255 , 0.7 );
            cairo_context->rectangle( box_start_x , box_start_y , box_width , box_height );
            cairo_context->fill_preserve();
            cairo_context->set_source_rgba( 0 , 0 , 0 , 1.0 );
            cairo_context->stroke();
            //draw content
            size_t item_total = game_object->menu_items.size();
            double item_size = box_height/item_total;
            for ( size_t i = 0 ; i < item_total ; i++ )
            {
                if ( i == game_object->focus_item_id )
                {
                    cairo_context->set_source_rgba( 255/255.0 , 125/255.0 , 0/255.0 , 1.0 );
                    cairo_context->set_line_width( 2 );
                    cairo_context->rectangle( box_start_x + 2 , i*item_size + box_start_y + 2 , box_width - 4 , item_size - 4 );
                    cairo_context->stroke();
                }
                std::string menu_name = game_object->menu_items[i].first();
                this->layout->set_text( menu_name );
                int layout_width = 0;
                int layout_height = 0;
                this->layout->get_pixel_size( layout_width , layout_height );
                int content_start_x = ( box_width - layout_width )/2 + box_start_x;
                int content_start_y = box_start_y + i*item_size + ( item_size - layout_height )/2;
                cairo_context->move_to( content_start_x ,  content_start_y );
                cairo_context->set_source_rgb( 1.0 , 1.0 , 1.0 );
                this->layout->show_in_cairo_context( cairo_context );
                cairo_context->fill_preserve();
                cairo_context->set_line_width( 0.5 );
                cairo_context->stroke();
            }
            cairo_context->restore();

            return false;
        }

        //draw signal handler end
        bool draw_message( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            GameEnvironment * game_object = this->game_object;
            Gdk::Rectangle maximum_rectangle = this->game_area->get_allocation();
            switch ( game_object->game_status )
            {
                case GAME_STATUS::MESSAGE:
                case GAME_STATUS::GAME_LOSE:
                case GAME_STATUS::GAME_WIN:
                    break;
                default:
                    return true;
            }
            if ( game_object->game_message.empty() )
                return true;

            const int widget_width = maximum_rectangle.get_width();
            const int widget_height = maximum_rectangle.get_height();
            this->layout->set_text( game_object->game_message.front() );
            int layout_width = 0;
            int layout_height = 0;
            this->layout->get_pixel_size( layout_width , layout_height );

            double box_width = layout_width;
            double box_height = layout_height;
            if ( box_width*3/2 < widget_width )
                box_width = widget_width*2/3;
            if ( box_height*6 < widget_height )
                box_height = widget_height/6;
            const int box_start_x = ( widget_width - box_width )/2;
            const int box_start_y = ( widget_height - box_height )/2;
            int content_start_x = ( box_width - layout_width )/2 + box_start_x;
            int content_start_y = ( box_height - layout_height )/2 + box_start_y;

            cairo_context->save();
            //draw box
            cairo_context->set_line_width( 2 );
            cairo_context->set_source_rgba( 43.0/255 , 42.0/255 , 43.0/255 , 0.7 );
            cairo_context->rectangle( box_start_x , box_start_y , box_width , box_height );

            cairo_context->fill_preserve();
            cairo_context->set_source_rgba( 0 , 0 , 0 , 1.0 );
            cairo_context->stroke();
            //draw content
            cairo_context->move_to( content_start_x ,  content_start_y );
            cairo_context->set_source_rgb( 1.0 , 1.0 , 1.0 );
            cairo_context->set_line_width( 0.5 );
            this->layout->show_in_cairo_context( cairo_context );
            cairo_context->fill_preserve();
            cairo_context->stroke();
            cairo_context->restore();
            return true;
        }

        void draw_dialog( const Cairo::RefPtr<Cairo::Context> & cairo_context , std::string text , std::uint32_t x , std::uint32_t y )
        {
            GameEnvironment * game_object = this->game_object;
            this->layout->set_text( text );
            int layout_width , layout_height;
            this->layout->get_pixel_size( layout_width , layout_height );

            //blank size
            layout_width += this->pixel_size/2;
            layout_height += this->pixel_size/2;

            if ( static_cast<std::uint32_t>( x + layout_width ) > ( game_object->towers.LENGTH )*this->pixel_size )
                x -= layout_width;
            if ( static_cast<std::uint32_t>( y + layout_height ) > ( game_object->towers.WIDTH )*this->pixel_size)
                y -= layout_height;

            cairo_context->save();
            cairo_context->set_line_width( 2 );
            cairo_context->set_source_rgba( 43.0/255 , 42.0/255 , 43.0/255 , 0.8 );
            cairo_context->rectangle( x , y , layout_width , layout_height );

            cairo_context->fill_preserve();
            cairo_context->set_source_rgba( 0 , 0 , 0 , 1.0 );
            cairo_context->stroke();

            cairo_context->move_to( x + 2 + this->pixel_size/4 , y + 2 + this->pixel_size/4 );
            cairo_context->set_source_rgb( 0.8 , 0.6 , 0.8 );
            cairo_context->set_line_width( 0.5 );
            this->layout->show_in_cairo_context( cairo_context );
            cairo_context->fill_preserve();
            cairo_context->stroke();
            cairo_context->restore();
        }

        bool draw_info( const Cairo::RefPtr<Cairo::Context> & cairo_context )
        {
            Hero& hero = this->game_object->hero;
            //draw background image
            Gdk::Cairo::set_source_pixbuf( cairo_context , this->info_frame , 0.0 , 0.0 );
            cairo_context->paint();

            //draw text
            std::vector< std::pair<std::string , int >> arr =
            {
                { std::string( "第  " ) + std::to_string( hero.layers + 1 ) + std::string( "  层" ) , 2 },
                { std::string( "等   级:  " ) + std::to_string( hero.level ) , 0 },
                { std::string( "生命值:  " ) + std::to_string( hero.life ) , 0 },
                { std::string( "攻击力:  " ) + std::to_string( hero.attack ) , 0 },
                { std::string( "防御力:  " ) + std::to_string( hero.defense ) , 0 },
                { std::string( "金   币:  " ) + std::to_string( hero.gold ) , 0 },
                { std::string( "经验值:  " ) + std::to_string( hero.experience ) , 0 },
                { std::string( "黄钥匙:  " ) + std::to_string( hero.yellow_key ) , 0 },
                { std::string( "蓝钥匙:  " ) + std::to_string( hero.blue_key ) , 0 },
                { std::string( "红钥匙:  " ) + std::to_string( hero.red_key ) , 0 },
                { std::string( "游戏菜单(ESC)" ) , 2 },
                { std::string( "商店菜单(S/s)" ) , 2 },
                { std::string( "楼层跳跃/浏览器(J/j)" ) , 2 },
            };

            Gtk::DrawingArea * widget = nullptr;
            builder_refptr->get_widget( "info_area" , widget );
            if ( widget == nullptr )
                return true;
            Gtk::Allocation allocation = widget->get_allocation();
            const int widget_width = allocation.get_width();
            const int widget_height = allocation.get_height();
            std::size_t arr_size = arr.size();
            for ( std::size_t i = 0 ; i < arr_size ; i++ )
            {
                this->layout->set_text( arr[i].first );

                int pos = 0;
                int layout_width = 0 , layout_height = 0;
                this->layout->get_pixel_size( layout_width , layout_height );
                switch ( arr[i].second )
                {
                    case 0:
                        pos = 0;
                        break;
                    case 1:
                        pos = 0;
                        break;
                    case 2:
                        pos = ( widget_width - layout_width )/2.0;
                        break;
                    case 3:
                        pos = widget_width - layout_width;
                        break;
                    default :
                        pos = 0;
                        break;
                }

                cairo_context->save();
                cairo_context->move_to( pos , widget_height/arr_size*i );
                cairo_context->set_source_rgb( 0.4 , 0.3 , 0.4 );
                cairo_context->set_line_width( 0.5 );
                this->layout->show_in_cairo_context( cairo_context );
                cairo_context->fill_preserve();
                cairo_context->stroke();
                cairo_context->restore();
            }

            return true;
        }

        bool scroll_signal_handler( GdkEventScroll * event )
        {
            GameEnvironment * game_object = this->game_object;
            switch ( game_object->game_status )
            {
                case GAME_STATUS::START_MENU:
                case GAME_STATUS::GAME_MENU:
                case GAME_STATUS::STORE_MENU:
                case GAME_STATUS::JUMP_MENU:
                {
                    switch ( event->direction )
                    {
                        case GDK_SCROLL_UP:
                        {
                            if ( game_object->focus_item_id > 0 )
                                game_object->focus_item_id--;
                            else
                                game_object->focus_item_id = game_object->menu_items.size() - 1;
                            break;
                        }
                        case GDK_SCROLL_DOWN:
                        {
                            if ( game_object->focus_item_id < game_object->menu_items.size() - 1 )
                                game_object->focus_item_id++;
                            else
                                game_object->focus_item_id = 0;
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                    break;
                }    
                default :
                {
                    break;
                }
            }

            return true;
        }

        bool key_press_handler( GdkEventKey * event )
        {
            GameEnvironment * game_object = this->game_object;
            switch ( game_object->game_status )
            {
                case GAME_STATUS::DIALOG:
                case GAME_STATUS::REVIEW_DETAIL:
                {
                    game_object->game_status = GAME_STATUS::NORMAL;
                    break;
                }
                case GAME_STATUS::NORMAL:
                {
                    switch( event->keyval )
                    {
                        case GDK_KEY_Left:
                        {
                            ( game_object->hero ).x -= 1;
                            bool flags = trigger_collision_event( game_object , this->script_engines.get() );
                            if ( flags == false )
                                ( game_object->hero ).x += 1;
                            break;
                        }
                        case GDK_KEY_Right:
                        {
                            ( game_object->hero ).x += 1;
                            bool flags = trigger_collision_event( game_object , this->script_engines.get() );
                            if ( flags == false )
                                ( game_object->hero ).x -= 1;
                            break;
                        }
                        case GDK_KEY_Up:
                        {
                            ( game_object->hero ).y -= 1;
                            bool flags = trigger_collision_event( game_object , this->script_engines.get() );
                            if ( flags == false )
                                ( game_object->hero ).y += 1;
                            break;
                        }
                        case GDK_KEY_Down:
                        {
                            ( game_object->hero ).y += 1;
                            bool flags = trigger_collision_event( game_object , this->script_engines.get() );
                            if ( flags == false )
                                ( game_object->hero ).y -= 1;
                            break;
                        }
                        case GDK_KEY_Escape:
                        {
                            open_game_menu_v2( game_object );
                            break;
                        }
                        case GDK_KEY_s:
                        case GDK_KEY_S:
                        {
                            open_store_menu_v2( game_object );
                            break;
                        }
                        case GDK_KEY_j:
                        case GDK_KEY_J:
                        {
                            open_layer_jump( game_object );
                            break;
                        }
                        case GDK_KEY_N:
                        case GDK_KEY_n:
                        {
                            game_object->music.play_next();
                        }
                        default :
                            break;
                    }
                    break;
                }
                case GAME_STATUS::START_MENU:
                {
                    switch( event->keyval )
                    {
                        case GDK_KEY_Up:
                        {
                            if ( game_object->focus_item_id > 0 )
                                game_object->focus_item_id--;
                            else
                                game_object->focus_item_id = game_object->menu_items.size() - 1;
                            break;
                        }
                        case GDK_KEY_Down:
                        {
                            if ( game_object->focus_item_id < game_object->menu_items.size() - 1 )
                                game_object->focus_item_id++;
                            else
                                game_object->focus_item_id = 0;
                            break;
                        }
                        case GDK_KEY_Return:
                        {
                            ( game_object->menu_items[ game_object->focus_item_id ] ).second();
                            break;
                        }
                        default :
                            break;
                    }
                    break;
                }
                case GAME_STATUS::MESSAGE:
                {
                    //check don't send message
                    if ( game_object->game_message.empty() )
                        game_object->game_status = GAME_STATUS::NORMAL;
                    game_object->game_message.pop_front();
                    if ( game_object->game_message.empty() )
                        game_object->game_status = GAME_STATUS::NORMAL;
                    break;
                }
                case GAME_STATUS::GAME_MENU:
                {
                    switch( event->keyval )
                    {
                        case GDK_KEY_Up:
                        {
                            if ( game_object->focus_item_id > 0 )
                                game_object->focus_item_id--;
                            else
                                game_object->focus_item_id = game_object->menu_items.size() - 1;
                            break;
                        }
                        case GDK_KEY_Down:
                        {
                            if ( game_object->focus_item_id < game_object->menu_items.size() - 1 )
                                game_object->focus_item_id++;
                            else
                                game_object->focus_item_id = 0;
                            break;
                        }
                        case GDK_KEY_Return:
                        {
                            ( game_object->menu_items[ game_object->focus_item_id ] ).second();
                            break;
                        }
                        case GDK_KEY_Escape:
                        {
                            close_game_menu_v2( game_object );
                            break;
                        }
                        default :
                            break;
                    }
                    break;
                }
                case GAME_STATUS::STORE_MENU:
                {
                    switch( event->keyval )
                    {
                        case GDK_KEY_Up:
                        {
                            if ( game_object->focus_item_id > 0 )
                                game_object->focus_item_id--;
                            else
                                game_object->focus_item_id = game_object->menu_items.size() - 1;
                            break;
                        }
                        case GDK_KEY_Down:
                        {
                            if ( game_object->focus_item_id < game_object->menu_items.size() - 1 )
                                game_object->focus_item_id++;
                            else
                                game_object->focus_item_id = 0;
                            break;
                        }
                        case GDK_KEY_Return:
                        {
                            ( game_object->menu_items[ game_object->focus_item_id ] ).second();
                            break;
                        }
                        case GDK_KEY_s:
                        case GDK_KEY_S:
                        {
                            close_store_menu_v2( game_object );
                            break;
                        }
                    }
                    break;
                }
                case GAME_STATUS::JUMP_MENU:
                {
                    switch( event->keyval )
                    {
                        case GDK_KEY_Up:
                        {
                            if ( game_object->focus_item_id > 0 )
                                game_object->focus_item_id--;
                            else
                                game_object->focus_item_id = game_object->menu_items.size() - 1;
                            break;
                        }
                        case GDK_KEY_Down:
                        {
                            if ( game_object->focus_item_id < game_object->menu_items.size() - 1 )
                                game_object->focus_item_id++;
                            else
                                game_object->focus_item_id = 0;
                            break;
                        }
                        case GDK_KEY_Return:
                        {
                            ( game_object->menu_items[ game_object->focus_item_id ] ).second();
                            break;
                        }
                        case GDK_KEY_j:
                        case GDK_KEY_J:
                        {
                            back_jump( game_object );
                            close_layer_jump( game_object );
                            break;
                        }
                    }
                    break;
                }
                case GAME_STATUS::GAME_LOSE:
                case GAME_STATUS::GAME_WIN:
                {
                    //check don't send message
                    if ( game_object->game_message.empty() )
                        open_start_menu( game_object );
                    game_object->game_message.pop_front();
                    if ( game_object->game_message.empty() )
                        open_start_menu( game_object );
                    break;
                }
                default:
                {
                    break;
                }
            }

            return true;
        }

        bool button_press_handler( GdkEventButton * event )
        {
            GameEnvironment * game_object = this->game_object;
            gint x = event->x , y = event->y;
            //gdk_window_get_device_position( event->window , event->device , &x , &y , nullptr );

            switch ( event->type )
            {
                case GDK_2BUTTON_PRESS:
                case GDK_3BUTTON_PRESS:
                {
                    break;
                }
                case GDK_BUTTON_PRESS:
                {
                    switch ( game_object->game_status )
                    {
                        case GAME_STATUS::REVIEW_DETAIL:
                        case GAME_STATUS::DIALOG:
                        {
                            game_object->game_status = GAME_STATUS::NORMAL;
                            break;
                        }
                        case GAME_STATUS::MESSAGE:
                        {
                            //check don't send message
                            if ( game_object->game_message.empty() )
                                game_object->game_status = GAME_STATUS::NORMAL;
                            game_object->game_message.pop_front();
                            if ( game_object->game_message.empty() )
                                game_object->game_status = GAME_STATUS::NORMAL;
                            break;
                        }
                        //if event->button == 1,behavior equal to GAME_STATUS::FIND_PATH,so don't break.
                        #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
                        case GAME_STATUS::NORMAL:
                        {
                            if ( event->button == 3 )
                            {
                                this->click_x = x;
                                this->click_y = y;
                                game_object->game_status = GAME_STATUS::REVIEW_DETAIL;
                                break;
                            }
                        }
                        #pragma GCC diagnostic warning "-Wimplicit-fallthrough" 
                        case GAME_STATUS::FIND_PATH:
                        {
                            if ( event->button == 1 )
                            {
                                game_object->path = find_path( game_object , { x/this->pixel_size , y/this->pixel_size } );
                                game_object->game_status = GAME_STATUS::FIND_PATH;
                                break;
                            }
                            break;
                        }
                        case GAME_STATUS::START_MENU:
                        case GAME_STATUS::STORE_MENU:
                        case GAME_STATUS::GAME_MENU:
                        case GAME_STATUS::JUMP_MENU:
                        {
                            Gdk::Rectangle menu_ractangle = this->get_menu_ractangle();
                            if ( x > menu_ractangle.get_x() &&
                                 x - menu_ractangle.get_width() < menu_ractangle.get_x() &&
                                 y > menu_ractangle.get_y() &&
                                 y - menu_ractangle.get_height() < menu_ractangle.get_y() )
                            {
                                size_t item_total = game_object->menu_items.size();
                                size_t access_index = ( y - menu_ractangle.get_y() )*item_total/menu_ractangle.get_height();
                                ( game_object->menu_items[ access_index ] ).second();
                            }
                            break;
                        }
                        case GAME_STATUS::GAME_LOSE:
                        case GAME_STATUS::GAME_WIN:
                        {
                            //check don't send message
                            if ( game_object->game_message.empty() )
                                open_start_menu( game_object );
                            game_object->game_message.pop_front();
                            if ( game_object->game_message.empty() )
                                open_start_menu( game_object );
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }
                default:
                {
                    break;
                }
            }

            return true;
        }

        void scriptengines_register_eventfunc( void )
        {
            //void set_tips( string )
            lua_register( this->script_engines.get() , "set_tips" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_set_tips" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );

                    const char * tips = luaL_checkstring( L , 1 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                    set_tips( game_object , tips );
                    return 0;
                }
            );

            //void set_grid_type( number layer , number x , number y , number grid_id )
            lua_register( this->script_engines.get() , "set_grid_type" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 4 )
                    {
                        g_log( "lua_set_grid_type" , G_LOG_LEVEL_WARNING , "expecting exactly 4 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 4 );

                    std::uint32_t layer = luaL_checkinteger( L , 1 );
                    std::uint32_t x = luaL_checkinteger( L , 2 );
                    std::uint32_t y = luaL_checkinteger( L , 3 );
                    std::uint32_t grid_id = luaL_checkinteger( L , 4 );
                    GRID_TYPE grid_type = static_cast<GRID_TYPE>( grid_id );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 5 );
                    
                    set_grid_type( game_object , { x , y , layer } , grid_type );
                    return 0;
                }
            );

            //number get_grid_type( number layer , number x , number y )
            lua_register( this->script_engines.get() , "get_grid_type" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 3 )
                    {
                        g_log( "lua_get_grid_type" , G_LOG_LEVEL_WARNING , "expecting exactly 3 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 3 );

                    std::uint32_t layer = luaL_checkinteger( L , 1 );
                    std::uint32_t x = luaL_checkinteger( L , 2 );
                    std::uint32_t y = luaL_checkinteger( L , 3 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 4 );

                    TowerGrid& grid = get_tower_grid( game_object->towers , x , y , layer );
                    lua_pushnumber( L , grid.type );
                    return 1;
                }
            );

            //table get_hero_property( void )
            lua_register( this->script_engines.get() , "get_hero_property" ,
                []( lua_State * L ) -> int
                {
                    //arguments number impossible less than 0,don't need check
                    //discard any extra arguments passed
                    lua_settop( L , 0 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );

                    Hero& hero = game_object->hero;
                    lua_newtable( L );
                    lua_pushnumber( L , hero.layers );
                    lua_setfield( L , -2 , "layers" );
                    lua_pushnumber( L , hero.x );
                    lua_setfield( L , -2 , "x" );
                    lua_pushnumber( L , hero.y );
                    lua_setfield( L , -2 , "y" );
                    lua_pushnumber( L , hero.level );
                    lua_setfield( L , -2 , "level" );
                    lua_pushnumber( L , hero.life );
                    lua_setfield( L , -2 , "life" );
                    lua_pushnumber( L , hero.attack );
                    lua_setfield( L , -2 , "attack" );
                    lua_pushnumber( L , hero.defense );
                    lua_setfield( L , -2 , "defense" );
                    lua_pushnumber( L , hero.gold );
                    lua_setfield( L , -2 , "gold" );
                    lua_pushnumber( L , hero.experience );
                    lua_setfield( L , -2 , "experience" );
                    lua_pushnumber( L , hero.yellow_key );
                    lua_setfield( L , -2 , "yellow_key" );
                    lua_pushnumber( L , hero.blue_key );
                    lua_setfield( L , -2 , "blue_key" );
                    lua_pushnumber( L , hero.red_key );
                    lua_setfield( L , -2 , "red_key" );
                    return 1;
                }
            );

            //void set_hero_property( table )
            lua_register( this->script_engines.get() , "set_hero_property" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_set_hero_property" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );

                    luaL_checktype( L , 1 , LUA_TTABLE );
                    lua_getfield( L , 1 , "layers" );
                    lua_getfield( L , 1 , "x" );
                    lua_getfield( L , 1 , "y" );
                    lua_getfield( L , 1 , "level" );
                    lua_getfield( L , 1 , "life" );
                    lua_getfield( L , 1 , "attack" );
                    lua_getfield( L , 1 , "defense" );
                    lua_getfield( L , 1 , "gold" );
                    lua_getfield( L , 1 , "experience" );
                    lua_getfield( L , 1 , "yellow_key" );
                    lua_getfield( L , 1 , "blue_key" );
                    lua_getfield( L , 1 , "red_key" );

                    game_object->hero.layers = luaL_checkinteger( L , -12 );
                    game_object->hero.x = luaL_checkinteger( L , -11 );
                    game_object->hero.y = luaL_checkinteger( L , -10 );
                    game_object->hero.level = luaL_checkinteger( L , -9 );
                    game_object->hero.life = luaL_checkinteger( L , -8 );
                    game_object->hero.attack = luaL_checkinteger( L , -7 );
                    game_object->hero.defense = luaL_checkinteger( L , -6 );
                    game_object->hero.gold = luaL_checkinteger( L , -5 );
                    game_object->hero.experience = luaL_checkinteger( L , -4 );
                    game_object->hero.yellow_key = luaL_checkinteger( L , -3 );
                    game_object->hero.blue_key = luaL_checkinteger( L , -2 );
                    game_object->hero.red_key = luaL_checkinteger( L , -1 );

                    return 0;
                }
            );

            //number get_flag( string flag_name )
            //if flag not exist,reutn nil
            lua_register( this->script_engines.get() , "get_flag" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_get_flag" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );

                    std::string flags_name( luaL_checkstring( L , 1 ) );
                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );

                    if ( game_object->script_flags.find( flags_name ) == game_object->script_flags.end() )
                    {
                        lua_pushnil( L );
                        return 1;
                    }
                    std::int64_t value = game_object->script_flags[flags_name];
                    lua_pushnumber( L , value );

                    return 1;
                }
            );

            //void set_flag( string flag_name , number flag_value )
            lua_register( this->script_engines.get() , "set_flag" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 2 )
                    {
                        g_log( "lua_set_flag" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 2 );

                    std::string flags_name( luaL_checkstring( L , 1 ) );
                    luaL_checktype( L , 2 , LUA_TNUMBER );
                    std::int64_t value = lua_tointeger( L , 2 );
                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 3 );
                    game_object->script_flags[flags_name] = value;

                    return 0;
                }
            );

            //void open_dialog( string list dialog_content )
            lua_register( this->script_engines.get() , "open_dialog" ,
                []( lua_State * L ) -> int
                {
                    std::size_t argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_get_item" , G_LOG_LEVEL_WARNING , "expecting exactly >= 1 arguments" );
                        return 0;
                    }
                    std::deque<std::string> messages;
                    for ( std::size_t i = 1 ; i <= argument_count ; i++ )
                    {
                        messages.push_back( luaL_checkstring( L , i ) );
                    }
                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , argument_count + 1 );
                    game_object->game_message = messages;

                    game_object->game_status = GAME_STATUS::MESSAGE;

                    return 0;
                }
            );

            //void open_menu( table of ( string item_name , string item_detail_json ) )
            lua_register( this->script_engines.get() , "open_menu" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_open_menu" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );
                    luaL_checktype( L , 1 , LUA_TTABLE );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                    game_object->menu_items = {};
                    lua_pop( L , 1 );

                    lua_pushnil( L );
                    while( lua_next( L , 1 ) )
                    {
                        if ( ( lua_type( L , -1 ) == LUA_TSTRING ) && ( lua_type( L , -2 ) == LUA_TSTRING ) )
                        {
                            std::string item_name( lua_tostring( L , -2 ) );
                            std::string item_func( lua_tostring( L , -1 ) );
                            game_object->menu_items.push_back({
                                [ item_name ](){ return item_name; },
                                [ L , item_func ](){ luaL_dostring( L , item_func.c_str() ); }
                            });
                        }
                        lua_pop( L , 1 );
                    }
                    game_object->menu_items.push_back({
                        [](){ return std::string( "关闭菜单" ); },
                        [ game_object ](){ game_object->game_status = GAME_STATUS::NORMAL; }
                    });
                    game_object->game_status = GAME_STATUS::GAME_MENU;
                    game_object->focus_item_id = 0;

                    return 0;
                }
            );

            //void close_menu( void )
            lua_register( this->script_engines.get() , "close_menu" ,
                []( lua_State * L ) -> int
                {
                    lua_settop( L , 0 );
                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );
                    if ( game_object->game_status == GAME_STATUS::GAME_MENU )
                    {
                        game_object->game_status = GAME_STATUS::NORMAL;
                    }
                    return 0;
                }
            );

            //void get_item( number item_id )
            lua_register( this->script_engines.get() , "get_item" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_get_item" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );

                    std::uint32_t item_id = luaL_checkinteger( L , 1 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );
                    
                    get_item( game_object , item_id );
                    return 0;
                }
            );

            //void unlock_store( number store_id )
            lua_register( this->script_engines.get() , "unlock_store" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_unlock_store" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );

                    std::uint32_t store_id = luaL_checkinteger( L , 1 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );

                    game_object->store_list[ store_id ].usability = true;
                    std::string tips = std::string( "解锁商店:" ) + ( game_object->store_list[ store_id ] ).name;
                    set_tips( game_object , tips );
                    return 0;
                }
            );

            //void lock_store( number store_id )
            lua_register( this->script_engines.get() , "lock_store" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 1 )
                    {
                        g_log( "lua_lock_store" , G_LOG_LEVEL_WARNING , "expecting exactly 1 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 1 );

                    std::uint32_t store_id = luaL_checkinteger( L , 1 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 2 );

                    game_object->store_list[ store_id ].usability = false;
                    std::string tips = std::string( "锁定商店:" ) + ( game_object->store_list[ store_id ] ).name;
                    set_tips( game_object , tips );
                    return 0;
                }
            );

            //void move_hero( number layer , number x , number y )
            lua_register( this->script_engines.get() , "move_hero" ,
                []( lua_State * L ) -> int
                {
                    int argument_count = lua_gettop( L );
                    if ( argument_count < 3 )
                    {
                        g_log( "lua_move_hero" , G_LOG_LEVEL_WARNING , "expecting exactly 3 arguments" );
                        return 0;
                    }
                    //discard any extra arguments passed
                    lua_settop( L , 3 );

                    std::uint32_t layer = luaL_checkinteger( L , 1 );
                    std::uint32_t x = luaL_checkinteger( L , 2 );
                    std::uint32_t y = luaL_checkinteger( L , 3 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 4 );
                    
                    move_hero( game_object , { x , y , layer } );
                    return 0;
                }
            );

            //void game_win( void )
            lua_register( this->script_engines.get() , "game_win" ,
                []( lua_State * L ) -> int
                {
                    //arguments number impossible less than 0,don't need check
                    //discard any extra arguments passed
                    lua_settop( L , 0 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );
                    
                    game_win( game_object );
                    return 0;
                }
            );

            //void game_lose( void )
            lua_register( this->script_engines.get() , "game_lose" ,
                []( lua_State * L ) -> int
                {
                    //arguments number impossible less than 0,don't need check
                    //discard any extra arguments passed
                    lua_settop( L , 0 );

                    lua_getglobal( L , "Z2FtZV9vYmplY3QK" );
                    GameEnvironment * game_object = ( GameEnvironment * )lua_touserdata( L , 1 );
                    
                    game_lose( game_object );
                    return 0;
                }
            );

        }

        bool exit_game( GdkEventAny * )
        {
            this->game_object->game_status = GAME_END;
            return true;
        }

    private:
        GameEnvironment * game_object;
        std::unique_ptr< lua_State , decltype( &lua_close ) > script_engines;
        Gtk::Main main_loop;
        Pango::FontDescription font_desc;
        Glib::RefPtr<Pango::Layout> layout;
        Glib::RefPtr<Gtk::Builder> builder_refptr;
        Gtk::Window * window;
        Gtk::DrawingArea * game_area;
        Gtk::DrawingArea * info_area;
        std::map<std::string,Glib::RefPtr<Gdk::Pixbuf>> image_resource;
        Glib::RefPtr<Gdk::Pixbuf> info_frame;
        std::uint32_t pixel_size = 32;
        std::uint32_t click_x = 0;
        std::uint32_t click_y = 0;
    };

    GameWindow::GameWindow( std::string program_name )
    {
        std::unique_ptr< char , decltype( &g_free ) > self_dir_path( g_path_get_dirname( program_name.c_str() ) , g_free );
        g_chdir( self_dir_path.get() );
        this->imp_ptr = new GameWindowImp;
    }
    GameWindow::~GameWindow()
    {
        delete this->imp_ptr;
    }
    void GameWindow::run()
    {
        imp_ptr->run();
    }
}
