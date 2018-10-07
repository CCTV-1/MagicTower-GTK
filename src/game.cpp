#include <cstdlib>
#include <cstring>
#include <cinttypes>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>

#include "game_event.h"

//signal handlers
static gboolean draw_loop( gpointer data );
static gboolean automatic_movement( gpointer data );
/* static gboolean restart_game( GtkWidget * widget , gpointer data );
static gboolean resume_game( GtkWidget * widget , gpointer data ); */
static gboolean apply_game_data( GtkWidget * widget , gpointer data );
static gboolean sync_game_data( GtkWidget * widget , gpointer data );
static void game_exit( GtkWidget * widget , gpointer data );
static gboolean key_press_handler( GtkWidget * widget , GdkEventKey * event , gpointer data );
static gboolean button_press_handler( GtkWidget * widget , GdkEventMotion * event , gpointer data );
static gboolean draw_info( GtkWidget * widget , cairo_t * cairo , gpointer data );
static gboolean draw_tower( GtkWidget * widget , cairo_t * cairo , gpointer data );
/* static gboolean music_switch( GtkWidget * widget , gboolean state , gpointer data );
static gboolean path_line_switch( GtkWidget * widget , gboolean state , gpointer data );
static gboolean test_switch( GtkWidget * widget , gboolean state , gpointer data ); */

static void gdk_pixbuff_free( GdkPixbuf * pixbuf );
static std::shared_ptr<GdkPixbuf> make_info_basic_frame( MagicTower::Tower& towers );
/* static void draw_box( cairo_t * cairo , double x , double y , double width , double height , std::int32_t animation_value ); */
static void draw_damage( cairo_t * cairo , MagicTower::GameEnvironment * game_object , double x , double y , std::uint32_t monster_id );
static void draw_text( GtkWidget * widget , cairo_t * cairo , std::shared_ptr<PangoFontDescription> font_desc , double x , double y , std::shared_ptr<gchar> text , int mode = 0 );
static void draw_dialog( cairo_t * cairo , MagicTower::GameEnvironment * game_object , std::shared_ptr<gchar> text , double x , double y );
static void draw_menu( cairo_t * cairo , MagicTower::GameEnvironment * game_object );
static void draw_tips( cairo_t * cairo , MagicTower::GameEnvironment * game_object );

static std::vector<std::shared_ptr<const char> > get_music_uris( const char * music_path );
static std::map<std::string,std::shared_ptr<GdkPixbuf> > laod_image_resource( const char * image_path );

int main( int argc , char * argv[] )
{
    std::shared_ptr<char> self_dir_path(
        g_path_get_dirname( argv[0] ),
        []( char * path ){ g_free( path ); }
    );
    g_chdir( self_dir_path.get() );

    std::vector<std::shared_ptr<const char> > music_list = get_music_uris( MUSIC_RESOURCES_PATH );
    MagicTower::Music music( &argc , &argv , music_list );
    /* music.play_start(); */

    MagicTower::DataBase db( DATABSE_RESOURCES_PATH );
    MagicTower::Tower towers = db.get_tower_info( 0 );
    MagicTower::Hero hero = db.get_hero_info( 0 );
    auto stairs = db.get_stairs_list();
    auto store_list = db.get_store_list();
    auto monsters = db.get_monster_list();
    auto items = db.get_item_list();
    auto custom_events = db.get_custom_events();
    MagicTower::Menu_t menu;

    gtk_init( &argc , &argv );

    GdkDisplay * default_display = gdk_display_get_default();
    GdkMonitor * monitor = gdk_display_get_monitor( default_display , 0 );
    GdkRectangle workarea;
    gdk_monitor_get_workarea( monitor , &workarea );
    int screen_width = workarea.width;
    int screen_height = workarea.height;
    int grid_width = screen_width/towers.LENGTH/3*2;
    int grid_height = screen_height/towers.WIDTH;
    if ( grid_width > grid_height )
        grid_width = grid_height;
    grid_width = grid_width/32*32;
    MagicTower::TOWER_GRID_SIZE = grid_width;

    std::map<std::string,std::shared_ptr<GdkPixbuf> > image_resource = laod_image_resource( IMAGE_RESOURCES_PATH );

    GtkBuilder * builder = gtk_builder_new_from_file( UI_DEFINE_RESOURCES_PATH );
    GtkWidget * game_window = GTK_WIDGET( gtk_builder_get_object( builder , "game_window" ) );
    GtkWidget * game_grid   = GTK_WIDGET( gtk_builder_get_object( builder , "game_grid" ) );
    GtkWidget * info_area   = GTK_WIDGET( gtk_builder_get_object( builder , "info_area"  ) );
    GtkWidget * tower_area  = GTK_WIDGET( gtk_builder_get_object( builder , "tower_area" ) );

    /* GtkWidget * exit_button_1         = GTK_WIDGET( gtk_builder_get_object( builder , "exit_game_button_1" ) );
    GtkWidget * exit_button_2            = GTK_WIDGET( gtk_builder_get_object( builder , "exit_game_button_2" ) );
    GtkWidget * restart_button           = GTK_WIDGET( gtk_builder_get_object( builder , "restart_game_button" ) );
    GtkWidget * resume_button            = GTK_WIDGET( gtk_builder_get_object( builder , "resume_game_button" ) );
    GtkWidget * music_setting_button     = GTK_WIDGET( gtk_builder_get_object( builder , "music_setting_button" ) );
    GtkWidget * path_line_setting_button = GTK_WIDGET( gtk_builder_get_object( builder , "path_line_setting_button" ) );
    GtkWidget * test_mode_setting_button = GTK_WIDGET( gtk_builder_get_object( builder , "test_mode_setting_button" ) ); */

    GtkWidget * apply_modify_button      = GTK_WIDGET( gtk_builder_get_object( builder , "apply_modify_button" ) );
    GtkWidget * synchronize_data_button  = GTK_WIDGET( gtk_builder_get_object( builder , "synchronize_data_button" ) );
    GtkWidget * layer_spin_button        = GTK_WIDGET( gtk_builder_get_object( builder , "layer_spin_button" ) );
    GtkWidget * x_spin_button            = GTK_WIDGET( gtk_builder_get_object( builder , "x_spin_button" ) );
    GtkWidget * y_spin_button            = GTK_WIDGET( gtk_builder_get_object( builder , "y_spin_button" ) );

    GtkAdjustment * layer_adjustment = gtk_adjustment_new( 1 , 1 , towers.HEIGHT , 1 , 10 , 0 );
    GtkAdjustment * x_adjustment     = gtk_adjustment_new( 0 , 0 , towers.LENGTH - 1 , 1 , 10 , 0 );
    GtkAdjustment * y_adjustment     = gtk_adjustment_new( 0 , 0 , towers.WIDTH - 1 , 1 , 10 , 0 );

    gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( layer_spin_button ) , GTK_ADJUSTMENT( layer_adjustment ) );
    gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( x_spin_button ) , GTK_ADJUSTMENT( x_adjustment ) );
    gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( y_spin_button ) , GTK_ADJUSTMENT( y_adjustment ) );

    //only call once
    std::shared_ptr<GdkPixbuf> info_frame = make_info_basic_frame( towers );

    std::shared_ptr<PangoFontDescription> damage_font(
        pango_font_description_from_string( "Microsoft YaHei UI 12" )
        , []( PangoFontDescription * desc ){ pango_font_description_free( desc ); }
    );
    std::shared_ptr<PangoFontDescription> info_font(
        pango_font_description_from_string( "Microsoft YaHei UI 16" )
        , []( PangoFontDescription * desc ){ pango_font_description_free( desc ); }
    );

    MagicTower::GameEnvironment game_object =
    {
        builder , info_frame , damage_font , info_font , {} , image_resource , custom_events , menu , 0 , music , hero ,
        towers , stairs , monsters , items , {} , store_list , MagicTower::GAME_STATUS::NORMAL , true , 0 , 0 , 0
    };

    //test mode window signal handler
    g_signal_connect( G_OBJECT( apply_modify_button ) , "clicked" , G_CALLBACK( apply_game_data ) , &game_object );
    g_signal_connect( G_OBJECT( synchronize_data_button ) , "clicked" , G_CALLBACK( sync_game_data ) , &game_object );

    //menu signal handler
    /* g_signal_connect( G_OBJECT( exit_button_1 ) , "clicked" , G_CALLBACK( game_exit ) , NULL );
    g_signal_connect( G_OBJECT( restart_button ) , "clicked" , G_CALLBACK( restart_game ) , &game_object );
    g_signal_connect( G_OBJECT( exit_button_2 ) , "clicked" , G_CALLBACK( game_exit ) , NULL );
    g_signal_connect( G_OBJECT( resume_button ) , "clicked" , G_CALLBACK( resume_game ) , &game_object );
    g_signal_connect( G_OBJECT( music_setting_button ) , "state-set" , G_CALLBACK( music_switch ) , &game_object );
    g_signal_connect( G_OBJECT( path_line_setting_button ) , "state-set" , G_CALLBACK( path_line_switch ) , &game_object );
    g_signal_connect( G_OBJECT( test_mode_setting_button ) , "state-set" , G_CALLBACK( test_switch ) , &game_object ); */

    //game signal handler
    g_signal_connect( G_OBJECT( info_area ) , "draw" , G_CALLBACK( draw_info ) , &game_object );
    g_signal_connect( G_OBJECT( tower_area ) , "draw" , G_CALLBACK( draw_tower ) , &game_object );
    g_signal_connect( G_OBJECT( tower_area ) , "button-press-event" , G_CALLBACK( button_press_handler ) , &game_object );
    g_signal_connect( G_OBJECT( game_window ) , "destroy" , G_CALLBACK( game_exit ) , NULL );
    g_signal_connect( G_OBJECT( game_window ) , "key-press-event" , G_CALLBACK( key_press_handler ) , &game_object );
    gtk_widget_set_events( tower_area , gtk_widget_get_events( tower_area )
        | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK 
        | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK );

    gtk_widget_set_size_request( GTK_WIDGET( tower_area ) , ( towers.LENGTH )*MagicTower::TOWER_GRID_SIZE , ( towers.WIDTH )*MagicTower::TOWER_GRID_SIZE );
    gtk_widget_set_size_request( GTK_WIDGET( info_area ) , ( towers.LENGTH/2 )*MagicTower::TOWER_GRID_SIZE , ( towers.WIDTH )*MagicTower::TOWER_GRID_SIZE );
    gtk_widget_set_size_request( GTK_WIDGET( game_grid ) , ( towers.LENGTH + towers.LENGTH/2 )*MagicTower::TOWER_GRID_SIZE , ( towers.WIDTH )*MagicTower::TOWER_GRID_SIZE );
    gtk_widget_set_size_request( GTK_WIDGET( game_window ) , ( towers.LENGTH + towers.LENGTH/2 )*MagicTower::TOWER_GRID_SIZE , ( towers.WIDTH )*MagicTower::TOWER_GRID_SIZE );
    gtk_widget_show_all( game_window );

    g_timeout_add( 50 , draw_loop , &game_object );
    g_timeout_add( 100 , automatic_movement , &game_object );
    gtk_main();

    return EXIT_SUCCESS;
}

/* static gboolean restart_game( GtkWidget * widget , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    if ( game_object->game_status != MagicTower::GAME_STATUS::GAME_LOSE &&
		game_object->game_status != MagicTower::GAME_STATUS::GAME_WIN )
            return TRUE;

    //reset game status
    MagicTower::DataBase db( DATABSE_RESOURCES_PATH );
    game_object->hero = db.get_hero_info( 0 );
    game_object->towers = db.get_tower_info( 0 );
    game_object->towers = db.get_tower_info( 0 );
    game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
    game_object->store_list = db.get_store_list();
    game_object->custom_events = db.get_custom_events();
    GtkWidget * game_window = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_window" ) );
    GtkWidget * game_grid = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_grid" ) );
    GtkWidget * game_end_menu = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "game_end_menu" ) );
    
    
    gtk_container_remove( GTK_CONTAINER( game_window ) , game_end_menu );
    gtk_container_add( GTK_CONTAINER( game_window ) , game_grid );
    gtk_widget_show( game_grid );
    return TRUE;
} */

/* static gboolean resume_game( GtkWidget * widget , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    close_game_menu( game_object );
    return FALSE;
} */

/* static gboolean test_switch( GtkWidget * widget , gboolean state , gpointer data )
{
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    GtkWidget * test_mode_window = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "test_mode_window" ) );
    GtkWidget * test_func_grid = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "test_func_grid" ) );

    if ( state == true )
    {
        if ( gtk_widget_get_visible( GTK_WIDGET( test_mode_window ) ) )
            return TRUE;

        sync_game_data( widget , data );

        gtk_widget_show_all( GTK_WIDGET( test_func_grid ) );
        gtk_widget_show_all( GTK_WIDGET( test_mode_window ) );
    }
    else if ( gtk_widget_get_visible( GTK_WIDGET( test_mode_window ) ) )
        gtk_widget_hide( GTK_WIDGET( test_mode_window ) );
    return FALSE;
} */

/* static gboolean music_switch( GtkWidget * widget , gboolean state , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    if ( state == true )
        game_object->music.play_resume();
    else
        game_object->music.play_pause();
    return FALSE;
} */

/* static gboolean path_line_switch( GtkWidget * widget , gboolean state , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    game_object->draw_path = state;
    return FALSE;
} */

static gboolean draw_tower( GtkWidget * widget , cairo_t * cairo , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    //GDK coordinate origin : Top left corner,left -> right x add,up -> down y add.
    auto draw_grid = [ &game_object , cairo ]( std::uint32_t x , std::uint32_t y , const char * image_type , std::uint32_t image_id )
    {
        std::shared_ptr<char> image_file_name(
            g_strdup_printf( IMAGE_RESOURCES_PATH"%s%" PRIu32 ".png" , image_type , image_id ) ,
            []( char * image_file_name ){ g_free( image_file_name ); }
        );
        const GdkPixbuf * element = ( game_object->image_resource[ image_file_name.get() ] ).get();
        if ( element == NULL )
        {
            g_log( __func__ , G_LOG_LEVEL_WARNING , "\'%s\' not be image file" , image_file_name.get() );
            return ;
        }
        gdk_cairo_set_source_pixbuf( cairo , element , x*( MagicTower::TOWER_GRID_SIZE ) , y*( MagicTower::TOWER_GRID_SIZE ) );
        cairo_paint( cairo );
    };

    //draw maps
    for( size_t y = 0 ; y < game_object->towers.LENGTH ; y++ )
    {
        for ( size_t x = 0 ; x < game_object->towers.WIDTH ; x++ )
        {
            auto grid = get_tower_grid( game_object->towers, game_object->hero.layers , x , y );
            switch( grid.type )
            {
                case MagicTower::GRID_TYPE::IS_FLOOR:
                {
                    draw_grid( x , y , "floor" , grid.id );
                    break;
                }
                case MagicTower::GRID_TYPE::IS_WALL:
                {
                    draw_grid( x , y , "wall" , grid.id );
                    break;
                }
                case MagicTower::GRID_TYPE::IS_STAIRS:
                {
                    draw_grid( x , y , "floor" , 1 );
                    draw_grid( x , y , "stairs" , game_object->stairs[ grid.id - 1 ].type );
                    break;
                }
                case MagicTower::GRID_TYPE::IS_DOOR:
                {
                    draw_grid( x , y , "floor" , 1 );
                    draw_grid( x , y , "door" , grid.id );
                    break;
                }
                case MagicTower::GRID_TYPE::IS_NPC:
                {
                    draw_grid( x , y , "floor" , 1 );
                    draw_grid( x , y , "npc" , grid.id );
                    break;
                }
                case MagicTower::GRID_TYPE::IS_MONSTER:
                {
                    draw_grid( x , y , "floor" , 1 );
                    draw_grid( x , y , "monster" , grid.id );
                    break;
                }
                case MagicTower::GRID_TYPE::IS_ITEM:
                {
                    draw_grid( x , y , "floor" , 1 );
                    draw_grid( x , y , "item" , grid.id );
                    break;
                }
                default :
                {
                    draw_grid( x , y , "boundary" , 1 );
                    break;
                }
            }
        }
    }

    //draw path line
    if ( ( game_object->game_status == MagicTower::GAME_STATUS::FIND_PATH ) &&
         !( game_object->path.empty() ) &&
         ( game_object->draw_path == true )
       )
    {
        cairo_save( cairo );
        cairo_set_source_rgba( cairo , 1 , 0.2 , 0.2 , 1.0 );
        cairo_set_line_width( cairo , 4.0 );
        cairo_arc( cairo , ( ( game_object->path[0] ).x + 0.5 )*MagicTower::TOWER_GRID_SIZE , ( ( game_object->path[0] ).y + 0.5 )*MagicTower::TOWER_GRID_SIZE 
            , 0.1*MagicTower::TOWER_GRID_SIZE , 0 , 2*G_PI );
        cairo_fill( cairo );
        for ( auto point : game_object->path )
        {
            cairo_line_to( cairo , ( ( point ).x + 0.5 )*MagicTower::TOWER_GRID_SIZE , ( ( point ).y + 0.5 )*MagicTower::TOWER_GRID_SIZE );
            cairo_move_to( cairo , ( ( point ).x + 0.5 )*MagicTower::TOWER_GRID_SIZE , ( ( point ).y + 0.5 )*MagicTower::TOWER_GRID_SIZE );
        }
        cairo_line_to( cairo , ( ( game_object->hero ).x + 0.5 )*MagicTower::TOWER_GRID_SIZE , ( ( game_object->hero ).y + 0.5 )*MagicTower::TOWER_GRID_SIZE );
        cairo_stroke( cairo );
        cairo_restore( cairo );
    }

    //draw hero
    draw_grid( ( game_object->hero ).x , ( game_object->hero ).y , "hero" , 1 );

    //draw battle damage
    for( size_t y = 0 ; y < game_object->towers.LENGTH ; y++ )
    {
        for ( size_t x = 0 ; x < game_object->towers.WIDTH ; x++ )
        {
            auto grid = get_tower_grid( game_object->towers, game_object->hero.layers , x , y );
            if ( grid.type == MagicTower::GRID_TYPE::IS_MONSTER )
            {
                draw_damage( cairo , game_object , x*MagicTower::TOWER_GRID_SIZE , ( y + 0.5 )*MagicTower::TOWER_GRID_SIZE , grid.id );
            }
        }
    }

    if ( game_object->tips_content.empty() == false )
        draw_tips( cairo , game_object );
    
    if ( game_object->game_status == MagicTower::GAME_STATUS::REVIEW_DETAIL )
    {
        std::int32_t x , y;
        std::string detail_str;
        const char * str;
        x = game_object->mouse_x - 0.5*MagicTower::TOWER_GRID_SIZE;
        x = x/MagicTower::TOWER_GRID_SIZE;
        y = game_object->mouse_y - 0.5*MagicTower::TOWER_GRID_SIZE;
        y = y/MagicTower::TOWER_GRID_SIZE;
        auto grid = get_tower_grid( game_object->towers , game_object->hero.layers , x , y );
        if ( grid.type == MagicTower::GRID_TYPE::IS_MONSTER )
        {
            auto monster = game_object->monsters[ grid.id - 1 ];
            detail_str = dump_monster_info( monster );
            str = detail_str.c_str();
        }
        else if( grid.type == MagicTower::GRID_TYPE::IS_ITEM )
        {
            auto item = game_object->items[ grid.id - 1 ];
            detail_str = dump_item_info( item );
            str = detail_str.c_str();
        }
        else
        {
            game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
            return TRUE;
        }
        std::shared_ptr<gchar> text( g_strdup_printf( str ) , []( gchar * text ){ g_free( text ); } );
        draw_dialog( cairo , game_object , text , game_object->mouse_x - 0.5*MagicTower::TOWER_GRID_SIZE , game_object->mouse_y - 0.5*MagicTower::TOWER_GRID_SIZE );
    }
    else if ( game_object->game_status == MagicTower::GAME_STATUS::STORE_MENU ||
              game_object->game_status == MagicTower::GAME_STATUS::GAME_MENU
    )
    {
        draw_menu( cairo , game_object );
    }

    return TRUE;
}

static gboolean draw_info( GtkWidget * widget , cairo_t * cairo , gpointer data )
{
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    MagicTower::Hero& hero = game_object->hero;
    //Draw background image
    gdk_cairo_set_source_pixbuf( cairo , game_object->info_frame.get() , 0 , 0 );
    cairo_paint( cairo );

    //Draw text
    //if remove ' ' to '%''PRIu32',g++ warning:
    //invalid suffix on literal; C++11 requires a space between literal and string macro.

    std::vector< std::pair<gchar * , int > > arr =
    {
        { g_strdup_printf( "第  %" PRIu32 "  层" , hero.layers + 1 ) , 2 },
        { g_strdup_printf( "等   级:  %" PRIu32 , hero.level ) , 0 },
        { g_strdup_printf( "生命值:  %" PRIu32 , hero.life ) , 0 },
        { g_strdup_printf( "攻击力:  %" PRIu32 , hero.attack ) , 0 },
        { g_strdup_printf( "防御力:  %" PRIu32 , hero.defense ) , 0 },
        { g_strdup_printf( "金   币:  %" PRIu32 , hero.gold ) , 0 },
        { g_strdup_printf( "经验值:  %" PRIu32 , hero.experience ) , 0 },
        { g_strdup_printf( "黄钥匙:  %" PRIu32 , hero.yellow_key ) , 0 },
        { g_strdup_printf( "蓝钥匙:  %" PRIu32 , hero.blue_key ) , 0 },
        { g_strdup_printf( "红钥匙:  %" PRIu32 , hero.red_key ) , 0 },
        { g_strdup_printf( "游戏菜单(ESC) " ) , 2 },
        { g_strdup_printf( "商店菜单(s/S)" ) , 2 },
        { g_strdup_printf( "楼层跳跃器(j/J)" ) , 2 },
    };

    int widget_height = gtk_widget_get_allocated_height( widget );
    std::size_t arr_size = arr.size();
    for ( std::size_t i = 0 ; i < arr_size ; i++ )
    {
        std::shared_ptr<gchar> text(
            arr[i].first , []( gchar * text ){ g_free( text ); }
        );
        draw_text( widget , cairo , game_object->info_font , 0 , widget_height/arr_size*i , text , arr[i].second );
    }

    return TRUE;
}

static gboolean key_press_handler( GtkWidget * widget , GdkEventKey * event , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    
    switch ( game_object->game_status )
    {
        case MagicTower::GAME_STATUS::DIALOG:
        case MagicTower::GAME_STATUS::REVIEW_DETAIL:
        {
            game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
            break;
        }
        case MagicTower::GAME_STATUS::NORMAL:
        {
            switch( event->keyval )
            {
                case GDK_KEY_Left:
                {
                    ( game_object->hero ).x -= 1;
                    bool flags = MagicTower::trigger_collision_event( game_object );
                    if ( flags == false )
                        ( game_object->hero ).x += 1;
                    break;
                }
                case GDK_KEY_Right:
                {
                    ( game_object->hero ).x += 1;
                    bool flags = MagicTower::trigger_collision_event( game_object );
                    if ( flags == false )
                        ( game_object->hero ).x -= 1;
                    break;
                }
                case GDK_KEY_Up:
                {
                    ( game_object->hero ).y -= 1;
                    bool flags = MagicTower::trigger_collision_event( game_object );
                    if ( flags == false )
                        ( game_object->hero ).y += 1;
                    break;
                }
                case GDK_KEY_Down:
                {
                    ( game_object->hero ).y += 1;
                    bool flags = MagicTower::trigger_collision_event( game_object );
                    if ( flags == false )
                        ( game_object->hero ).y -= 1;
                    break;
                }
                case GDK_KEY_Escape:
                {
                    MagicTower::open_game_menu_v2( game_object );
                    break;
                }
                case GDK_KEY_s:
                case GDK_KEY_S:
                {
                    MagicTower::open_store_menu_v2( game_object );
                    break;
                }
                case GDK_KEY_j:
                case GDK_KEY_J:
                {
                    MagicTower::open_layer_jump( game_object );
                    break;
                }
                default :
                    break;
            }
            break;
        }
        case MagicTower::GAME_STATUS::GAME_MENU:
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
                    MagicTower::close_game_menu_v2( game_object );
                    break;
                }
                default :
                    break;
            }
            break;
        }
        case MagicTower::GAME_STATUS::STORE_MENU:
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
                    MagicTower::close_store_menu_v2( game_object );
                    break;
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }

    return FALSE;
}

static inline gboolean automatic_movement( gpointer data )
{
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    if ( game_object->game_status != MagicTower::GAME_STATUS::FIND_PATH )
    {
        return TRUE;
    }
    if ( game_object->path.empty() )
    {
        game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
        return TRUE;
    }
    MagicTower::TowerGridLocation goal = game_object->path.back();
    MagicTower::TowerGridLocation temp = { ( game_object->hero ).x , ( game_object->hero ).y };
    ( game_object->hero ).x = goal.x;
    ( game_object->hero ).y = goal.y;
    bool flags = MagicTower::trigger_collision_event( game_object );
    if ( flags == false )
    {
        ( game_object->hero ).x = temp.x;
        ( game_object->hero ).y = temp.y;
        //if goal is npc,game status status set to dialog,now can't set game status to NORMAL
        if ( game_object->game_status == MagicTower::GAME_STATUS::FIND_PATH )
            game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
        return TRUE;
    }
    game_object->path.pop_back();
    return TRUE;
}

static gboolean button_press_handler( GtkWidget * widget , GdkEventMotion * event , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    GdkEventButton * mouse_event = reinterpret_cast<GdkEventButton *>( event );
    gint x , y;
    GdkModifierType state;  
    gdk_window_get_device_position( event->window , event->device , &x , &y , &state );

    switch ( mouse_event->type )
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
                case MagicTower::GAME_STATUS::REVIEW_DETAIL:
                case MagicTower::GAME_STATUS::DIALOG:
                {
                    game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
                    break;
                }
                //if mouse_event->button == 1,behavior equal to MagicTower::GAME_STATUS::FIND_PATH,so don't break.
                #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
                case MagicTower::GAME_STATUS::NORMAL:
                {
                    if ( mouse_event->button == 3 )
                    {
                        game_object->mouse_x = x + 0.5*MagicTower::TOWER_GRID_SIZE;
                        game_object->mouse_y = y + 0.5*MagicTower::TOWER_GRID_SIZE;
                        game_object->game_status = MagicTower::GAME_STATUS::REVIEW_DETAIL;
                        break;
                    }
                }
                #pragma GCC diagnostic warning "-Wimplicit-fallthrough" 
                case MagicTower::GAME_STATUS::FIND_PATH:
                {
                    if ( mouse_event->button == 1 )
                    {
                        game_object->path = MagicTower::find_path( game_object , { x/MagicTower::TOWER_GRID_SIZE , y/MagicTower::TOWER_GRID_SIZE } );
                        game_object->game_status = MagicTower::GAME_STATUS::FIND_PATH;
                        break;
                    }
                    break;
                }
                case MagicTower::GAME_STATUS::STORE_MENU:
                case MagicTower::GAME_STATUS::GAME_MENU:
                {
                    int widget_width = gtk_widget_get_allocated_width( widget );
                    int widget_height = gtk_widget_get_allocated_height( widget );
                    const int box_start_x = widget_height/6;
                    const int box_start_y = widget_width/6;
	                double box_height = widget_height*2/3;
	                double box_width = widget_width*2/3;
	                if ( x > box_start_x && x - box_width < box_start_x && y > box_start_y && y - box_height < box_start_y )
	                {
	                	size_t item_total = game_object->menu_items.size();
	                	size_t access_index = ( y - box_start_y )*item_total/box_height;
	                	( game_object->menu_items[ access_index ] ).second();
	                }
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

    return TRUE;
}

static gboolean draw_loop( gpointer data )
{
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    if ( game_object->game_status == MagicTower::GAME_STATUS::GAME_LOSE &&
        game_object->game_status  == MagicTower::GAME_STATUS::GAME_WIN
    )
        return TRUE;
    
    GtkWidget * info_area  = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "info_area"  ) );
    GtkWidget * tower_area = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );
    gtk_widget_queue_draw( info_area );
    gtk_widget_queue_draw( tower_area );
    return TRUE;
}

static gboolean apply_game_data( GtkWidget * widget , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );

    GtkWidget * layer_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "layer_spin_button" ) );
    GtkWidget * x_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "x_spin_button" ) );
    GtkWidget * y_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "y_spin_button" ) );
    GtkWidget * level_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "level_spin_button" ) );
    GtkWidget * life_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "life_spin_button" ) );
    GtkWidget * attack_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "attack_spin_button" ) );
    GtkWidget * defense_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "defense_spin_button" ) );
    GtkWidget * gold_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "gold_spin_button" ) );
    GtkWidget * experience_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "experience_spin_button" ) );
    GtkWidget * yellow_key_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "yellow_key_spin_button" ) );
    GtkWidget * blue_key_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "blue_key_spin_button" ) );
    GtkWidget * red_key_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "red_key_spin_button" ) );

    game_object->hero.x = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( x_spin_button ) ) );
    game_object->hero.layers = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( layer_spin_button ) ) - 1 );
    game_object->hero.y = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( y_spin_button ) ) );
    game_object->hero.level = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( level_spin_button ) ) );
    game_object->hero.life = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( life_spin_button ) ) );
    game_object->hero.attack = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( attack_spin_button ) ) );
    game_object->hero.defense = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( defense_spin_button ) ) );
    game_object->hero.gold = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( gold_spin_button ) ) );
    game_object->hero.experience = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( experience_spin_button ) ) );
    game_object->hero.yellow_key = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( yellow_key_spin_button ) ) );
    game_object->hero.blue_key = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( blue_key_spin_button ) ) );
    game_object->hero.red_key = static_cast<std::uint32_t>( gtk_spin_button_get_value( GTK_SPIN_BUTTON( red_key_spin_button ) ) );

    return TRUE;
}

static gboolean sync_game_data( GtkWidget * widget , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    
    GtkWidget * layer_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "layer_spin_button" ) );
    GtkWidget * x_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "x_spin_button" ) );
    GtkWidget * y_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "y_spin_button" ) );
    GtkWidget * level_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "level_spin_button" ) );
    GtkWidget * life_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "life_spin_button" ) );
    GtkWidget * attack_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "attack_spin_button" ) );
    GtkWidget * defense_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "defense_spin_button" ) );
    GtkWidget * gold_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "gold_spin_button" ) );
    GtkWidget * experience_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "experience_spin_button" ) );
    GtkWidget * yellow_key_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "yellow_key_spin_button" ) );
    GtkWidget * blue_key_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "blue_key_spin_button" ) );
    GtkWidget * red_key_spin_button = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "red_key_spin_button" ) );
    
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( layer_spin_button ) , static_cast<gdouble>( game_object->hero.layers ) + 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( x_spin_button ) , static_cast<gdouble>( game_object->hero.x ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( y_spin_button ) , static_cast<gdouble>( game_object->hero.y ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( level_spin_button ) , static_cast<gdouble>( game_object->hero.level ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( life_spin_button ) , static_cast<gdouble>( game_object->hero.life ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( attack_spin_button ) , static_cast<gdouble>( game_object->hero.attack ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( defense_spin_button ) , static_cast<gdouble>( game_object->hero.defense ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( gold_spin_button ) , static_cast<gdouble>( game_object->hero.gold ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( experience_spin_button ) , static_cast<gdouble>( game_object->hero.experience ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( yellow_key_spin_button ) , static_cast<gdouble>( game_object->hero.yellow_key ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( blue_key_spin_button ) , static_cast<gdouble>( game_object->hero.blue_key ) );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( red_key_spin_button ) , static_cast<gdouble>( game_object->hero.red_key ) );

    return FALSE;
}

static std::map<std::string,std::shared_ptr<GdkPixbuf> > laod_image_resource( const char * image_path )
{
    std::shared_ptr<GDir> dir_ptr(
        g_dir_open( image_path , 0 , NULL ) ,
        []( GDir * dir_ptr ){ g_dir_close( dir_ptr ); }
    );
    std::map<std::string,std::shared_ptr<GdkPixbuf> > image_resource;

    //exists and type check
    if ( g_file_test( image_path , G_FILE_TEST_EXISTS ) != TRUE )
        return {};
    if ( g_file_test( image_path , G_FILE_TEST_IS_DIR ) != TRUE )
        return {};

    //g_dir_read_name auto ignore "." ".."
    const gchar * filename;
    while ( ( filename = g_dir_read_name( dir_ptr.get() ) ) != NULL )
    {
        std::shared_ptr<char> image_file_name(
            g_strdup_printf( IMAGE_RESOURCES_PATH"%s" , filename ) ,
            []( char * image_file_name ){ g_free( image_file_name ); }
        );
        GdkPixbuf * pixbuf = gdk_pixbuf_new_from_file_at_size( image_file_name.get() , MagicTower::TOWER_GRID_SIZE , MagicTower::TOWER_GRID_SIZE , NULL );
        if ( pixbuf == NULL )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "\'%s\' not be image file,ignore this." , image_file_name.get() );
            continue;
        }
        std::shared_ptr<GdkPixbuf> image_pixbuf( pixbuf , gdk_pixbuff_free );
        image_resource.insert( std::pair< std::string , std::shared_ptr<GdkPixbuf> >( std::string( image_file_name.get() ) , image_pixbuf ) );
    }
    return image_resource;
}

static std::vector<std::shared_ptr<const char> > get_music_uris( const char * music_path )
{
    std::shared_ptr<GDir> dir_ptr(
        g_dir_open( music_path , 0 , NULL ) ,
        []( GDir * dir_ptr ){ g_dir_close( dir_ptr ); }
    );

    if ( g_file_test( music_path , G_FILE_TEST_EXISTS ) != TRUE )
        return {};
    if ( g_file_test( music_path , G_FILE_TEST_IS_DIR ) != TRUE )
        return {};

    std::vector<std::shared_ptr<const char> > uri_array;

    //g_dir_read_name auto ignore "." ".."
    const gchar * filename;
    while ( ( filename = g_dir_read_name( dir_ptr.get() ) ) != NULL )
    {
        std::shared_ptr<char> music_file_name(
            g_strdup_printf( "%s/%s" , music_path , filename ) ,
            []( char * music_file_name ){ g_free( music_file_name ); }
        );
        std::shared_ptr<GFile> gfile_obj(
            g_file_new_for_path( music_file_name.get() ) ,
            []( GFile * gfile_obj ){ g_object_unref( gfile_obj ); }
        );
        std::shared_ptr<const char> file_uri(
            g_file_get_uri( gfile_obj.get() ) ,
            []( char * file_uri ){ g_free( file_uri ); }
        );
        uri_array.push_back( file_uri );
    }

    return uri_array;
}

static void gdk_pixbuff_free( GdkPixbuf * pixbuf )
{
    g_object_unref( pixbuf );
}

static void game_exit( GtkWidget * widget , gpointer data )
{
    ( void )data;
    gtk_widget_destroy( widget );
    gtk_main_quit();
}

static void draw_damage( cairo_t * cairo , MagicTower::GameEnvironment * game_object ,
    double x , double y , std::uint32_t monster_id )
{
    int64_t damage;
    GtkWidget * widget = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );
    
    damage = MagicTower::get_combat_damage( game_object , monster_id );
    std::shared_ptr<gchar> damage_text(
        ( damage >= 0 ) ? ( g_strdup_printf( "%" PRId64 , damage ) ) : ( g_strdup_printf( "????" ) ) ,
        []( gchar * damage_text ){ g_free( damage_text ); }
    );
    std::shared_ptr<PangoLayout> layout(
        gtk_widget_create_pango_layout( widget , damage_text.get() )
        ,  []( PangoLayout * layout ){ g_object_unref( layout ); }
    );
    std::shared_ptr<cairo_pattern_t> pattern(
        cairo_pattern_create_linear( 0.0 , 0.0 , MagicTower::TOWER_GRID_SIZE , MagicTower::TOWER_GRID_SIZE )
        , []( cairo_pattern_t * pattern ){ cairo_pattern_destroy( pattern ); }
    );
    pango_layout_set_font_description( layout.get() , game_object->damage_font.get() );
    cairo_move_to( cairo , x , y );
    pango_cairo_layout_path( cairo , layout.get() );
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
    cairo_pattern_add_color_stop_rgb( pattern.get() , 0.0 , red_value , green_value , 0.0 );
    cairo_set_source( cairo , pattern.get() );
    cairo_fill_preserve( cairo );
    cairo_set_line_width( cairo , 0.5 );
    cairo_stroke( cairo );
}

static void draw_tips( cairo_t * cairo , MagicTower::GameEnvironment * game_object )
{
    if ( game_object->tips_content.empty() )
        return ;

    GtkWidget * widget = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );
    int widget_width = gtk_widget_get_allocated_width( widget );
    int widget_height = gtk_widget_get_allocated_height( widget );
    std::shared_ptr<cairo_pattern_t> pattern(
        cairo_pattern_create_linear( 0.0 , 0.0 , widget_width , widget_height )
        , []( cairo_pattern_t * pattern ){ cairo_pattern_destroy( pattern ); }
    );

    std::size_t tips_size = game_object->tips_content.size();

    for ( std::size_t i = 0 ; i < tips_size ; i++ )
    {
        std::shared_ptr<PangoLayout> layout(
            gtk_widget_create_pango_layout( widget , game_object->tips_content[i].get() )
            ,  []( PangoLayout * layout ){ g_object_unref( layout ); }
        );
        pango_layout_set_font_description( layout.get() , game_object->info_font.get() );
        int layout_width , layout_height;
        pango_layout_get_pixel_size( layout.get() , &layout_width , &layout_height );
        cairo_move_to( cairo , 0 , 0 + i*layout_height );
        cairo_set_source_rgba( cairo , 43.0/255 , 42.0/255 , 43.0/255 , 0.7 );
        cairo_rel_line_to( cairo , layout_width , 0 );
        cairo_rel_line_to( cairo , 0 , layout_height );
        cairo_rel_line_to( cairo , -1*layout_width , 0 );
        cairo_close_path( cairo );

        cairo_set_line_width( cairo , 2 );
        cairo_fill_preserve( cairo );
        cairo_set_source_rgba( cairo , 0 , 0 , 0 , 1.0 );
        cairo_stroke( cairo );

        cairo_move_to( cairo , 0 ,  0 + i*layout_height );
        pango_cairo_layout_path( cairo , layout.get() );
        cairo_pattern_add_color_stop_rgb( pattern.get() , 0.0 , 1.0 , 1.0 , 1.0 );
        cairo_set_source( cairo , pattern.get() );
        cairo_fill_preserve( cairo );
        cairo_set_line_width( cairo , 0.5 );
        cairo_stroke( cairo );
    }

}

void draw_menu( cairo_t * cairo , MagicTower::GameEnvironment * game_object )
{
    GtkWidget * widget = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );
    int widget_width = gtk_widget_get_allocated_width( widget );
    int widget_height = gtk_widget_get_allocated_height( widget );
    const int box_start_x = widget_height/6;
    const int box_start_y = widget_width/6;

	//draw box
	double box_height = widget_height*2/3;
	double box_width = widget_width*2/3;
	cairo_move_to( cairo, box_start_x , box_start_y );
	cairo_set_source_rgba( cairo , 43.0/255 , 42.0/255 , 43.0/255 , 0.7 );
	cairo_rel_line_to( cairo , 0 , box_height );
	cairo_rel_line_to( cairo , box_width , 0 );
	cairo_rel_line_to( cairo , 0 , 0 - box_height );
	cairo_rel_line_to( cairo , 0 - box_width , 0 );
	cairo_close_path( cairo );
	cairo_set_line_width( cairo , 2 );
	cairo_fill_preserve( cairo );
	cairo_set_source_rgba( cairo , 0 , 0 , 0 , 1.0 );
	cairo_stroke( cairo );
	//draw content
	size_t item_total = game_object->menu_items.size();
	double item_size = box_height/item_total;
    std::shared_ptr<cairo_pattern_t> pattern(
        cairo_pattern_create_linear( 0.0 , 0.0 , widget_width , widget_height )
        , []( cairo_pattern_t * pattern ){ cairo_pattern_destroy( pattern ); }
    );
	for ( size_t i = 0 ; i < item_total ; i++ )
	{
		if ( i == game_object->focus_item_id )
		{
			cairo_move_to( cairo , box_start_x + 2 , i*item_size + box_start_y + 2 );
			cairo_rel_line_to( cairo , box_width - 4 , 0 );
			cairo_rel_line_to( cairo , 0 , item_size - 4 );
			cairo_rel_line_to( cairo , -1*box_width + 4 , 0 );
			cairo_rel_line_to( cairo , 0 , -1*item_size + 4 );
			cairo_set_source_rgba( cairo , 255/255.0 , 125/255.0 , 0/255.0 , 1.0 );
			cairo_set_line_width( cairo , 2 );
			cairo_stroke( cairo );
		}
        std::string menu_name = game_object->menu_items[i].first();
        std::shared_ptr<PangoLayout> layout(
            gtk_widget_create_pango_layout( widget , menu_name.c_str() )
            ,  []( PangoLayout * layout ){ g_object_unref( layout ); }
        );
		pango_layout_set_font_description( layout.get() , game_object->info_font.get() );
		int layout_width = 0;
        int layout_height = 0;
		pango_layout_get_pixel_size( layout.get() , &layout_width , &layout_height );
		int content_start_x = ( box_width - layout_width )/2 + box_start_x;
		int content_start_y = box_start_y + i*item_size + ( item_size - layout_height )/2;
		cairo_move_to( cairo , content_start_x ,  content_start_y );
		pango_cairo_layout_path( cairo , layout.get() );
		cairo_pattern_add_color_stop_rgb( pattern.get() , 0.0 , 1.0 , 1.0 , 1.0 );
		cairo_set_source( cairo , pattern.get() );
		cairo_fill_preserve( cairo );
		cairo_set_line_width( cairo , 0.5 );
		cairo_stroke( cairo );
	}
}

static void draw_dialog( cairo_t * cairo , MagicTower::GameEnvironment * game_object , std::shared_ptr<gchar> text , double x , double y )
{
    GtkWidget * widget = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );
    std::shared_ptr<PangoLayout> layout(
        gtk_widget_create_pango_layout( widget , text.get() )
        ,  []( PangoLayout * layout ){ g_object_unref( layout ); }
    );
    int widget_width = gtk_widget_get_allocated_width( widget );
    int widget_height = gtk_widget_get_allocated_height( widget );
    std::shared_ptr<cairo_pattern_t> pattern(
        cairo_pattern_create_linear( 0.0 , 0.0 , widget_width , widget_height )
        , []( cairo_pattern_t * pattern ){ cairo_pattern_destroy( pattern ); }
    );
    pango_layout_set_font_description( layout.get() , game_object->info_font.get() );
    int layout_width , layout_height;
    pango_layout_get_pixel_size( layout.get() , &layout_width , &layout_height );

    //blank size
    layout_width += MagicTower::TOWER_GRID_SIZE/2;
    layout_height += MagicTower::TOWER_GRID_SIZE/2;

    if ( static_cast<std::uint32_t>( x + layout_width ) > ( game_object->towers.LENGTH )*MagicTower::TOWER_GRID_SIZE )
        x -= layout_width;
    if ( static_cast<std::uint32_t>( y + layout_height ) > ( game_object->towers.WIDTH )*MagicTower::TOWER_GRID_SIZE)
        y -= layout_height;

    cairo_move_to( cairo , x , y );
    cairo_set_source_rgba( cairo , 43.0/255 , 42.0/255 , 43.0/255 , 0.8 );
    cairo_rel_line_to( cairo , layout_width , 0 );
    cairo_rel_line_to( cairo , 0 , layout_height );
    cairo_rel_line_to( cairo , -1*layout_width , 0 );
    cairo_close_path( cairo );

    cairo_set_line_width( cairo , 2 );
    cairo_fill_preserve( cairo );
    cairo_set_source_rgba( cairo , 0 , 0 , 0 , 1.0 );
    cairo_stroke( cairo );

    cairo_move_to( cairo , x + 2 + MagicTower::TOWER_GRID_SIZE/4 , y + 2 + MagicTower::TOWER_GRID_SIZE/4 );
    pango_cairo_layout_path( cairo , layout.get() );
    cairo_pattern_add_color_stop_rgb( pattern.get() , 0.0 , 0.8 , 0.6 , 0.8 );
    cairo_set_source( cairo , pattern.get() );
    cairo_fill_preserve( cairo );
    cairo_set_line_width( cairo , 0.5 );
    cairo_stroke( cairo );
}

/* static void draw_box( cairo_t * cairo , double x , double y , double width , double height , std::int32_t animation_value )
{
    cairo_move_to( cairo , x , y );
    cairo_set_source_rgba( cairo , 43.0/255 , 42.0/255 , 43.0/255 , 1.0/10*animation_value );
    cairo_rel_line_to( cairo , width/10*animation_value , 0 );
    cairo_rel_line_to( cairo , 0 , height/10*animation_value );
    cairo_rel_line_to( cairo , -1*width/10*animation_value , 0 );
    cairo_close_path( cairo );

    cairo_set_line_width( cairo , 2 );
    cairo_fill_preserve( cairo );
    cairo_set_source_rgba( cairo , 0 , 0 , 0 , 1.0/10*animation_value );
    cairo_stroke( cairo );
} */

//if mode != 0 ignore argument x,mode = 1 left alignment,mode = 2 center alignment,mode = 3 right alignment.
static void draw_text( GtkWidget * widget , cairo_t * cairo , std::shared_ptr<PangoFontDescription> font_desc ,
    double x , double y , std::shared_ptr<gchar> text , int mode )
{
    std::shared_ptr<PangoLayout> layout(
        gtk_widget_create_pango_layout( widget , text.get() )
        ,  []( PangoLayout * layout ){ g_object_unref( layout ); }
    );
    int widget_width = gtk_widget_get_allocated_width( widget );
    int widget_height = gtk_widget_get_allocated_height( widget );
    std::shared_ptr<cairo_pattern_t> pattern(
        cairo_pattern_create_linear( 0.0 , 0.0 , widget_width , widget_height )
        , []( cairo_pattern_t * pattern ){ cairo_pattern_destroy( pattern ); }
    );
    pango_layout_set_font_description( layout.get() , font_desc.get() );
    
    int layout_width;
    pango_layout_get_pixel_size( layout.get() , &layout_width , NULL );
    int pos;
    switch ( mode )
    {
        case 0:
            pos = x;
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

    cairo_move_to( cairo , pos , y );
    pango_cairo_layout_path( cairo , layout.get() );
    cairo_pattern_add_color_stop_rgb( pattern.get() , 0.0 , 0.4 , 0.3 , 0.4 );
    cairo_set_source( cairo , pattern.get() );
    cairo_fill_preserve( cairo );
    cairo_set_line_width( cairo , 0.5 );
    cairo_stroke( cairo );
}

static std::shared_ptr<GdkPixbuf> make_info_basic_frame( MagicTower::Tower& towers )
{
    std::shared_ptr<GdkPixbuf> info_frame(
        gdk_pixbuf_new( GDK_COLORSPACE_RGB , TRUE , 8 , ( towers.LENGTH/2 )*MagicTower::TOWER_GRID_SIZE , ( towers.WIDTH )*MagicTower::TOWER_GRID_SIZE ) ,
        gdk_pixbuff_free
    );

    GdkPixbuf * blank_pixbuf = gdk_pixbuf_new_from_file_at_size( IMAGE_RESOURCES_PATH"wall1.png" , MagicTower::TOWER_GRID_SIZE , MagicTower::TOWER_GRID_SIZE , NULL );
    if ( blank_pixbuf == NULL )
    {
        g_log( __func__ , G_LOG_LEVEL_WARNING , "\'%s\' not be image file" , IMAGE_RESOURCES_PATH"wall1.png" );
        return info_frame;
    }
    std::shared_ptr<GdkPixbuf> info_blank( blank_pixbuf , gdk_pixbuff_free );
    
    GdkPixbuf * bg_pixbuf = gdk_pixbuf_new_from_file_at_size( IMAGE_RESOURCES_PATH"floor11.png" , MagicTower::TOWER_GRID_SIZE , MagicTower::TOWER_GRID_SIZE , NULL );
    if( bg_pixbuf == NULL )
    {
        g_log( __func__ , G_LOG_LEVEL_WARNING , "\'%s\' not be image file" , IMAGE_RESOURCES_PATH"floor11.png" );
        return info_frame;
    }
    std::shared_ptr<GdkPixbuf> info_bg( bg_pixbuf , gdk_pixbuff_free );

    for ( size_t y = 0 ; y < towers.LENGTH ; y++ )
    {
        for ( size_t x = 0 ; x < towers.WIDTH/2 /* - 1 */ ; x++ )
        {
            gdk_pixbuf_composite( info_bg.get() , info_frame.get() ,
            x*( MagicTower::TOWER_GRID_SIZE ) , y*( MagicTower::TOWER_GRID_SIZE ) , MagicTower::TOWER_GRID_SIZE , MagicTower::TOWER_GRID_SIZE ,
            x*( MagicTower::TOWER_GRID_SIZE ) , y*( MagicTower::TOWER_GRID_SIZE ) , 1 , 1 , GDK_INTERP_NEAREST , 255 );
        }
    }

    return info_frame;
}
