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

static int PIXEL_SIZE;

std::shared_ptr<PangoFontDescription> damage_font(
    pango_font_description_from_string( "Microsoft YaHei UI 12" ),
    pango_font_description_free
);
std::shared_ptr<PangoFontDescription> info_font(
    pango_font_description_from_string( "Microsoft YaHei UI 16" ),
    pango_font_description_free
);
std::shared_ptr<PangoLayout> layout;

int mouse_x = 0;
int mouse_y = 0;

std::map<std::string,std::shared_ptr<GdkPixbuf> > image_resource;
std::shared_ptr<GdkPixbuf> info_frame;

//signal handlers
static gboolean draw_loop( gpointer data );
static gboolean automatic_movement( gpointer data );
static gboolean apply_game_data( GtkWidget * widget , gpointer data );
static gboolean sync_game_data( GtkWidget * widget , gpointer data );
static void game_exit( GtkWidget * widget , gpointer data );
static gboolean scroll_signal_handler( GtkWidget * widget , GdkEventScroll * event , gpointer data );
static gboolean key_press_handler( GtkWidget * widget , GdkEventKey * event , gpointer data );
static gboolean button_press_handler( GtkWidget * widget , GdkEventMotion * event , gpointer data );
static gboolean draw_info( GtkWidget * widget , cairo_t * cairo , gpointer data );
static gboolean draw_tower( GtkWidget * widget , cairo_t * cairo , gpointer data );

static void gdk_pixbuff_free( GdkPixbuf * pixbuf );
static void draw_path_line( cairo_t * cairo , MagicTower::GameEnvironment * game_object );
static void draw_detail( cairo_t * cairo , MagicTower::GameEnvironment * game_object );
static void draw_damage( cairo_t * cairo , MagicTower::GameEnvironment * game_object , double x , double y , std::uint32_t monster_id );
static void draw_text( GtkWidget * widget , cairo_t * cairo , std::shared_ptr<PangoFontDescription> font_desc , double x , double y , std::shared_ptr<gchar> text , int mode = 0 );
static void draw_message( cairo_t * cairo , MagicTower::GameEnvironment * game_object );
static void draw_dialog( cairo_t * cairo , MagicTower::GameEnvironment * game_object , std::shared_ptr<gchar> text , double x , double y );
static void draw_menu( cairo_t * cairo , MagicTower::GameEnvironment * game_object );
static void draw_tips( cairo_t * cairo , MagicTower::GameEnvironment * game_object );

static std::shared_ptr<GdkPixbuf> info_frame_factory( size_t width , size_t height );
static std::vector<std::shared_ptr<const char> > load_music( const char * music_path );
static std::map<std::string,std::shared_ptr<GdkPixbuf> > load_image_resource( const char * image_path );

int main( int argc , char * argv[] )
{
    gtk_init( &argc , &argv );
    std::shared_ptr<char> self_dir_path(
        g_path_get_dirname( argv[0] ),
        []( char * path ){ g_free( path ); }
    );
    g_chdir( self_dir_path.get() );

    std::vector<std::shared_ptr<const char> > music_list = load_music( MUSIC_RESOURCES_PATH );

    MagicTower::GameEnvironment game_object( music_list );

    GdkDisplay * default_display = gdk_display_get_default();
    GdkMonitor * monitor = gdk_display_get_monitor( default_display , 0 );
    GdkRectangle workarea;
    gdk_monitor_get_workarea( monitor , &workarea );
    int screen_width = workarea.width;
    int screen_height = workarea.height;
    int grid_width = screen_width/game_object.towers.LENGTH/3*2;
    int grid_height = screen_height/game_object.towers.WIDTH;
    if ( grid_width > grid_height )
        grid_width = grid_height;
    PIXEL_SIZE = grid_width/32*32;

    std::shared_ptr<PangoContext> context( pango_font_map_create_context(pango_cairo_font_map_get_default()), g_object_unref );
    std::shared_ptr<PangoLayout> temp( pango_layout_new( context.get() ) , g_object_unref );
    layout = std::move(temp);
    image_resource = load_image_resource( IMAGE_RESOURCES_PATH );
    info_frame = info_frame_factory( game_object.towers.WIDTH/2 , game_object.towers.HEIGHT );

    GtkWidget * layer_spin_button    = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "layer_spin_button" ) );
    GtkWidget * x_spin_button        = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "x_spin_button" ) );
    GtkWidget * y_spin_button        = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "y_spin_button" ) );
    GtkAdjustment * layer_adjustment = gtk_adjustment_new( 1 , 1 , game_object.towers.HEIGHT , 1 , 10 , 0 );
    GtkAdjustment * x_adjustment     = gtk_adjustment_new( 0 , 0 , game_object.towers.LENGTH - 1 , 1 , 10 , 0 );
    GtkAdjustment * y_adjustment     = gtk_adjustment_new( 0 , 0 , game_object.towers.WIDTH - 1 , 1 , 10 , 0 );
    gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( layer_spin_button ) , GTK_ADJUSTMENT( layer_adjustment ) );
    gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( x_spin_button ) , GTK_ADJUSTMENT( x_adjustment ) );
    gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON( y_spin_button ) , GTK_ADJUSTMENT( y_adjustment ) );

    //test mode window signal handler
    GtkWidget * apply_modify_button     = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "apply_modify_button" ) );
    GtkWidget * synchronize_data_button = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "synchronize_data_button" ) );
    g_signal_connect( G_OBJECT( apply_modify_button ) , "clicked" , G_CALLBACK( apply_game_data ) , &game_object );
    g_signal_connect( G_OBJECT( synchronize_data_button ) , "clicked" , G_CALLBACK( sync_game_data ) , &game_object );

    //game signal handler
    GtkWidget * game_window = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "game_window" ) );
    //GtkWidget * game_grid   = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "game_grid" ) );
    GtkWidget * info_area   = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "info_area"  ) );
    GtkWidget * tower_area  = GTK_WIDGET( gtk_builder_get_object( game_object.builder , "tower_area" ) );
    int tower_width = ( game_object.towers.LENGTH )*PIXEL_SIZE;
    int info_width = ( game_object.towers.LENGTH/2 )*PIXEL_SIZE;
    int window_height = ( game_object.towers.LENGTH )*PIXEL_SIZE;
    g_signal_connect( G_OBJECT( info_area ) , "draw" , G_CALLBACK( draw_info ) , &game_object );
    g_signal_connect( G_OBJECT( tower_area ) , "draw" , G_CALLBACK( draw_tower ) , &game_object );
    g_signal_connect( G_OBJECT( tower_area ) , "button-press-event" , G_CALLBACK( button_press_handler ) , &game_object );
    g_signal_connect( G_OBJECT( tower_area ) , "scroll-event" , G_CALLBACK( scroll_signal_handler ) , &game_object );
    g_signal_connect( G_OBJECT( game_window ) , "destroy" , G_CALLBACK( game_exit ) , nullptr );
    g_signal_connect( G_OBJECT( game_window ) , "key-press-event" , G_CALLBACK( key_press_handler ) , &game_object );
    gtk_widget_set_events( tower_area , gtk_widget_get_events( tower_area )
        | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK
        | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK );
    gtk_widget_set_size_request( GTK_WIDGET( tower_area ) , tower_width , window_height );
    gtk_widget_set_size_request( GTK_WIDGET( info_area ) ,info_width  , window_height );
    //gtk_widget_set_size_request( GTK_WIDGET( game_grid ) , tower_width + info_width , window_height );
    gtk_widget_set_size_request( GTK_WIDGET( game_window ) , tower_width + info_width , window_height );
    gtk_widget_show_all( game_window );

    g_timeout_add( 50 , draw_loop , &game_object );
    g_timeout_add( 100 , automatic_movement , &game_object );
    gtk_main();

    return EXIT_SUCCESS;
}

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
        const GdkPixbuf * element = ( image_resource[ image_file_name.get() ] ).get();
        if ( element == nullptr )
        {
            g_log( __func__ , G_LOG_LEVEL_WARNING , "\'%s\' not be image file" , image_file_name.get() );
            return ;
        }
        gdk_cairo_set_source_pixbuf( cairo , element , x*PIXEL_SIZE , y*PIXEL_SIZE );
        cairo_paint( cairo );
    };

    //draw maps
    for( size_t y = 0 ; y < game_object->towers.LENGTH ; y++ )
    {
        for ( size_t x = 0 ; x < game_object->towers.WIDTH ; x++ )
        {
            auto grid = get_tower_grid( game_object->towers , game_object->hero.layers , x , y );
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

    draw_path_line( cairo , game_object );

    //draw hero
    draw_grid( ( game_object->hero ).x , ( game_object->hero ).y , "hero" , 1 );

    //draw battle damage
    for( size_t y = 0 ; y < game_object->towers.LENGTH ; y++ )
    {
        for ( size_t x = 0 ; x < game_object->towers.WIDTH ; x++ )
        {
            auto grid = get_tower_grid( game_object->towers , game_object->hero.layers , x , y );
            if ( grid.type == MagicTower::GRID_TYPE::IS_MONSTER )
            {
                draw_damage( cairo , game_object , x*PIXEL_SIZE , ( y + 0.5 )*PIXEL_SIZE , grid.id );
            }
        }
    }

    draw_tips( cairo , game_object );
    draw_detail( cairo , game_object );
    draw_menu( cairo , game_object );
    draw_message( cairo , game_object );

    return TRUE;
}

static gboolean draw_info( GtkWidget * widget , cairo_t * cairo , gpointer data )
{
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    MagicTower::Hero& hero = game_object->hero;
    //Draw background image
    gdk_cairo_set_source_pixbuf( cairo , info_frame.get() , 0 , 0 );
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
        { g_strdup_printf( "游戏菜单(ESC)" ) , 2 },
        { g_strdup_printf( "商店菜单(s/S)" ) , 2 },
        { g_strdup_printf( "楼层跳跃/浏览器(j/J)" ) , 2 },
    };

    int widget_height = gtk_widget_get_allocated_height( widget );
    std::size_t arr_size = arr.size();
    for ( std::size_t i = 0 ; i < arr_size ; i++ )
    {
        std::shared_ptr<gchar> text(
            arr[i].first , []( gchar * text ){ g_free( text ); }
        );
        draw_text( widget , cairo , info_font , 0 , widget_height/arr_size*i , text , arr[i].second );
    }

    return TRUE;
}

static gboolean scroll_signal_handler( GtkWidget * widget , GdkEventScroll * event , gpointer data )
{
    ( void )widget;
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    switch ( game_object->game_status )
    {
        case MagicTower::GAME_STATUS::START_MENU:
        case MagicTower::GAME_STATUS::GAME_MENU:
        case MagicTower::GAME_STATUS::STORE_MENU:
        case MagicTower::GAME_STATUS::JUMP_MENU:
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
        case MagicTower::GAME_STATUS::MESSAGE:
        {
            //check don't send message
            if ( game_object->game_message.empty() )
                game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
            game_object->game_message.pop_front();
            if ( game_object->game_message.empty() )
                game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
            break;
        }
        case MagicTower::GAME_STATUS::START_MENU:
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
        case MagicTower::GAME_STATUS::JUMP_MENU:
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
                    MagicTower::back_jump( game_object );
                    MagicTower::close_layer_jump( game_object );
                    break;
                }
            }
            break;
        }
        case MagicTower::GAME_STATUS::GAME_LOSE:
        case MagicTower::GAME_STATUS::GAME_WIN:
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
                case MagicTower::GAME_STATUS::MESSAGE:
                {
                    //check don't send message
                    if ( game_object->game_message.empty() )
                        game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
                    game_object->game_message.pop_front();
                    if ( game_object->game_message.empty() )
                        game_object->game_status = MagicTower::GAME_STATUS::NORMAL;
                    break;
                }
                //if mouse_event->button == 1,behavior equal to MagicTower::GAME_STATUS::FIND_PATH,so don't break.
                #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
                case MagicTower::GAME_STATUS::NORMAL:
                {
                    if ( mouse_event->button == 3 )
                    {
                        mouse_x = x + 0.5*PIXEL_SIZE;
                        mouse_y = y + 0.5*PIXEL_SIZE;
                        game_object->game_status = MagicTower::GAME_STATUS::REVIEW_DETAIL;
                        break;
                    }
                }
                #pragma GCC diagnostic warning "-Wimplicit-fallthrough" 
                case MagicTower::GAME_STATUS::FIND_PATH:
                {
                    if ( mouse_event->button == 1 )
                    {
                        game_object->path = MagicTower::find_path( game_object , { x/PIXEL_SIZE , y/PIXEL_SIZE } );
                        game_object->game_status = MagicTower::GAME_STATUS::FIND_PATH;
                        break;
                    }
                    break;
                }
                case MagicTower::GAME_STATUS::START_MENU:
                case MagicTower::GAME_STATUS::STORE_MENU:
                case MagicTower::GAME_STATUS::GAME_MENU:
                case MagicTower::GAME_STATUS::JUMP_MENU:
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
                    break;
                }
                case MagicTower::GAME_STATUS::GAME_LOSE:
                case MagicTower::GAME_STATUS::GAME_WIN:
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

    return TRUE;
}

static gboolean draw_loop( gpointer data )
{
    MagicTower::GameEnvironment * game_object = static_cast<MagicTower::GameEnvironment *>( data );
    
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

static std::map<std::string,std::shared_ptr<GdkPixbuf> > load_image_resource( const char * image_path )
{
    std::shared_ptr<GDir> dir_ptr(
        g_dir_open( image_path , 0 , nullptr ) ,
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
    while ( ( filename = g_dir_read_name( dir_ptr.get() ) ) != nullptr )
    {
        std::shared_ptr<char> image_file_name(
            g_strdup_printf( IMAGE_RESOURCES_PATH"%s" , filename ) ,
            []( char * image_file_name ){ g_free( image_file_name ); }
        );
        GdkPixbuf * pixbuf = gdk_pixbuf_new_from_file_at_size( image_file_name.get() , PIXEL_SIZE , PIXEL_SIZE , nullptr );
        if ( pixbuf == nullptr )
        {
            g_log( __func__ , G_LOG_LEVEL_MESSAGE , "\'%s\' not be image file,ignore this." , image_file_name.get() );
            continue;
        }
        std::shared_ptr<GdkPixbuf> image_pixbuf( pixbuf , gdk_pixbuff_free );
        image_resource.insert({ std::string( image_file_name.get() ) , image_pixbuf });
    }
    return image_resource;
}

static std::vector<std::shared_ptr<const char> > load_music( const char * music_path )
{
    std::shared_ptr<GDir> dir_ptr(
        g_dir_open( music_path , 0 , nullptr ) ,
        []( GDir * dir_ptr ){ g_dir_close( dir_ptr ); }
    );

    if ( g_file_test( music_path , G_FILE_TEST_EXISTS ) != TRUE )
        return {};
    if ( g_file_test( music_path , G_FILE_TEST_IS_DIR ) != TRUE )
        return {};

    std::vector<std::shared_ptr<const char> > uri_array;

    //g_dir_read_name auto ignore "." ".."
    const gchar * filename;
    while ( ( filename = g_dir_read_name( dir_ptr.get() ) ) != nullptr )
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

static void draw_path_line( cairo_t * cairo , MagicTower::GameEnvironment * game_object )
{
    if ( game_object->game_status != MagicTower::GAME_STATUS::FIND_PATH )
        return ;
    if ( game_object->path.empty() )
        return ;
    if ( game_object->draw_path == false )
        return ;

    cairo_save( cairo );
    cairo_set_source_rgba( cairo , 1 , 0.2 , 0.2 , 1.0 );
    cairo_set_line_width( cairo , 4.0 );
    cairo_arc( cairo , ( ( game_object->path[0] ).x + 0.5 )*PIXEL_SIZE , ( ( game_object->path[0] ).y + 0.5 )*PIXEL_SIZE 
        , 0.1*PIXEL_SIZE , 0 , 2*G_PI );
    cairo_fill( cairo );
    for ( auto point : game_object->path )
    {
        cairo_line_to( cairo , ( ( point ).x + 0.5 )*PIXEL_SIZE , ( ( point ).y + 0.5 )*PIXEL_SIZE );
        cairo_move_to( cairo , ( ( point ).x + 0.5 )*PIXEL_SIZE , ( ( point ).y + 0.5 )*PIXEL_SIZE );
    }
    cairo_line_to( cairo , ( ( game_object->hero ).x + 0.5 )*PIXEL_SIZE , ( ( game_object->hero ).y + 0.5 )*PIXEL_SIZE );
    cairo_stroke( cairo );
    cairo_restore( cairo );
}

static void draw_detail( cairo_t * cairo , MagicTower::GameEnvironment * game_object )
{
    if ( game_object->game_status != MagicTower::GAME_STATUS::REVIEW_DETAIL )
        return ;

    std::int32_t x , y;
    std::string detail_str;
    const char * str;
    x = mouse_x - 0.5*PIXEL_SIZE;
    x = x/PIXEL_SIZE;
    y = mouse_y - 0.5*PIXEL_SIZE;
    y = y/PIXEL_SIZE;
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
        return ;
    }
    std::shared_ptr<gchar> text( g_strdup_printf( str ) , []( gchar * text ){ g_free( text ); } );
    draw_dialog( cairo , game_object , text , mouse_x - 0.5*PIXEL_SIZE , mouse_y - 0.5*PIXEL_SIZE );
}

static void draw_damage( cairo_t * cairo , MagicTower::GameEnvironment * game_object ,
    double x , double y , std::uint32_t monster_id )
{
    int64_t damage = MagicTower::get_combat_damage( game_object , monster_id );
    std::shared_ptr<gchar> damage_text(
        ( damage >= 0 ) ? ( g_strdup_printf( "%" PRId64 , damage ) ) : ( g_strdup_printf( "????" ) ) ,
        []( gchar * damage_text ){ g_free( damage_text ); }
    );
    pango_layout_set_text( layout.get() , damage_text.get() , -1 );
    pango_layout_set_font_description( layout.get() , damage_font.get() );

    cairo_save( cairo );
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
    cairo_set_source_rgb( cairo , red_value , green_value , 0.0 );
    cairo_fill_preserve( cairo );
    cairo_set_line_width( cairo , 0.5 );
    cairo_stroke( cairo );
    cairo_restore( cairo );
}

static void draw_message( cairo_t * cairo , MagicTower::GameEnvironment * game_object )
{
    switch ( game_object->game_status )
    {
        case MagicTower::GAME_STATUS::MESSAGE:
        case MagicTower::GAME_STATUS::GAME_LOSE:
        case MagicTower::GAME_STATUS::GAME_WIN:
            break;
        default:
            return ;
    }
    if ( game_object->game_message.empty() )
        return ;

    GtkWidget * widget = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );
    int widget_width = gtk_widget_get_allocated_width( widget );
    int widget_height = gtk_widget_get_allocated_height( widget );
    pango_layout_set_text( layout.get() , game_object->game_message.front().c_str() , -1 );
    pango_layout_set_font_description( layout.get() , info_font.get() );
    int layout_width = 0;
    int layout_height = 0;
    pango_layout_get_pixel_size( layout.get() , &layout_width , &layout_height );

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

    cairo_save( cairo );
    //draw box
    cairo_move_to( cairo , box_start_x , box_start_y );
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
    cairo_move_to( cairo , content_start_x ,  content_start_y );
    pango_cairo_layout_path( cairo , layout.get() );
    cairo_set_source_rgb( cairo , 1.0 , 1.0 , 1.0 );
    cairo_fill_preserve( cairo );
    cairo_set_line_width( cairo , 0.5 );
    cairo_stroke( cairo );
    cairo_restore( cairo );
}

static void draw_tips( cairo_t * cairo , MagicTower::GameEnvironment * game_object )
{
    if ( game_object->tips_content.empty() )
        return ;
    

    std::size_t tips_size = game_object->tips_content.size();
    pango_layout_set_font_description( layout.get() , info_font.get() );

    cairo_save( cairo );
    for ( std::size_t i = 0 ; i < tips_size ; i++ )
    {
        pango_layout_set_text( layout.get() , game_object->tips_content[i].get() , -1 );
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
        cairo_set_source_rgb( cairo , 1.0 , 1.0 , 1.0 );
        pango_cairo_layout_path( cairo , layout.get() );
        cairo_fill_preserve( cairo );
        cairo_set_line_width( cairo , 0.5 );
        cairo_stroke( cairo );
    }
    cairo_restore( cairo );

}

void draw_menu( cairo_t * cairo , MagicTower::GameEnvironment * game_object )
{
    switch ( game_object->game_status )
    {
        case MagicTower::GAME_STATUS::START_MENU:
        case MagicTower::GAME_STATUS::STORE_MENU :
        case MagicTower::GAME_STATUS::GAME_MENU :
        case MagicTower::GAME_STATUS::JUMP_MENU :
            break;
        default:
            return ;
    }

    GtkWidget * widget = GTK_WIDGET( gtk_builder_get_object( game_object->builder , "tower_area" ) );
    int widget_width = gtk_widget_get_allocated_width( widget );
    int widget_height = gtk_widget_get_allocated_height( widget );
    const int box_start_x = widget_height/6;
    const int box_start_y = widget_width/6;

    double box_height = widget_height*2/3;
    double box_width = widget_width*2/3;
    
    cairo_save( cairo );
    //draw box
    cairo_move_to( cairo , box_start_x , box_start_y );
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
    pango_layout_set_font_description( layout.get() , info_font.get() );
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
        pango_layout_set_text( layout.get() , menu_name.c_str() , -1 );
        int layout_width = 0;
        int layout_height = 0;
        pango_layout_get_pixel_size( layout.get() , &layout_width , &layout_height );
        int content_start_x = ( box_width - layout_width )/2 + box_start_x;
        int content_start_y = box_start_y + i*item_size + ( item_size - layout_height )/2;
        cairo_move_to( cairo , content_start_x ,  content_start_y );
        pango_cairo_layout_path( cairo , layout.get() );
        cairo_set_source_rgb( cairo , 1.0 , 1.0 , 1.0 );
        cairo_fill_preserve( cairo );
        cairo_set_line_width( cairo , 0.5 );
        cairo_stroke( cairo );
    }
    cairo_restore( cairo );
}

static void draw_dialog( cairo_t * cairo , MagicTower::GameEnvironment * game_object , std::shared_ptr<gchar> text , double x , double y )
{
    pango_layout_set_text( layout.get() , text.get() , -1 );
    pango_layout_set_font_description( layout.get() , info_font.get() );
    int layout_width , layout_height;
    pango_layout_get_pixel_size( layout.get() , &layout_width , &layout_height );

    //blank size
    layout_width += PIXEL_SIZE/2;
    layout_height += PIXEL_SIZE/2;

    if ( static_cast<std::uint32_t>( x + layout_width ) > ( game_object->towers.LENGTH )*PIXEL_SIZE )
        x -= layout_width;
    if ( static_cast<std::uint32_t>( y + layout_height ) > ( game_object->towers.WIDTH )*PIXEL_SIZE)
        y -= layout_height;
    
    cairo_save( cairo );
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

    cairo_move_to( cairo , x + 2 + PIXEL_SIZE/4 , y + 2 + PIXEL_SIZE/4 );
    pango_cairo_layout_path( cairo , layout.get() );
    cairo_set_source_rgb( cairo , 0.8 , 0.6 , 0.8 );
    cairo_fill_preserve( cairo );
    cairo_set_line_width( cairo , 0.5 );
    cairo_stroke( cairo );
    cairo_restore( cairo );
}

//if mode != 0 ignore argument x,mode = 1 left alignment,mode = 2 center alignment,mode = 3 right alignment.
static void draw_text( GtkWidget * widget , cairo_t * cairo , std::shared_ptr<PangoFontDescription> font_desc ,
    double x , double y , std::shared_ptr<gchar> text , int mode )
{
    pango_layout_set_text( layout.get() , text.get() , -1 );
    pango_layout_set_font_description( layout.get() , font_desc.get() );
    
    int pos;
    int layout_width;
    int widget_width = gtk_widget_get_allocated_width( widget );
    pango_layout_get_pixel_size( layout.get() , &layout_width , nullptr );
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

    cairo_save( cairo );
    cairo_move_to( cairo , pos , y );
    pango_cairo_layout_path( cairo , layout.get() );
    cairo_set_source_rgb( cairo , 0.4 , 0.3 , 0.4 );
    cairo_fill_preserve( cairo );
    cairo_set_line_width( cairo , 0.5 );
    cairo_stroke( cairo );
    cairo_restore( cairo );
}

static std::shared_ptr<GdkPixbuf> info_frame_factory( size_t width , size_t height )
{
    std::shared_ptr<GdkPixbuf> info_frame(
        gdk_pixbuf_new( GDK_COLORSPACE_RGB , TRUE , 8 , width*PIXEL_SIZE , height*PIXEL_SIZE ) ,
        gdk_pixbuff_free
    );

    GdkPixbuf * bg_pixbuf = gdk_pixbuf_new_from_file_at_size( IMAGE_RESOURCES_PATH"floor11.png" , PIXEL_SIZE , PIXEL_SIZE , nullptr );
    if( bg_pixbuf == nullptr )
    {
        g_log( __func__ , G_LOG_LEVEL_WARNING , "\'%s\' not be image file" , IMAGE_RESOURCES_PATH"floor11.png" );
        return info_frame;
    }
    std::shared_ptr<GdkPixbuf> info_bg( bg_pixbuf , gdk_pixbuff_free );

    for ( size_t y = 0 ; y < height ; y++ )
    {
        for ( size_t x = 0 ; x < width ; x++ )
        {
            gdk_pixbuf_composite( info_bg.get() , info_frame.get() ,
            x*PIXEL_SIZE , y*PIXEL_SIZE , PIXEL_SIZE , PIXEL_SIZE ,
            x*PIXEL_SIZE , y*PIXEL_SIZE , 1 , 1 , GDK_INTERP_NEAREST , 255 );
        }
    }

    return info_frame;
}
