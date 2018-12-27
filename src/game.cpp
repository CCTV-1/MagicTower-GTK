#include "game_window.h"

int main( int , char * argv[] )
{
    MagicTower::GameWindow game( argv[0] );
    game.run();

    return EXIT_SUCCESS;
}
