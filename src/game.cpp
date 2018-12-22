#include "game_window.h"

int main( int argc , char * argv[] )
{
    ( void )argc;

    MagicTower::GameWindow game( argv[0] );
    game.run();

    return EXIT_SUCCESS;
}
