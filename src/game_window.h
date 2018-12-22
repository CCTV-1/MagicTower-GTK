#pragma once
#ifndef GAME_WINDOW_H
#define GAME_WINDOW_H

#include <string>

namespace MagicTower
{
    class GameWindowImp;

    class GameWindow
    {
    public:
        GameWindow( std::string program_name = std::string( "./" ) );
        ~GameWindow();
        void run();

        GameWindow( const GameWindow& rhs )=delete;
        GameWindow( GameWindow&& rhs )=delete;
        GameWindow& operator=( const GameWindow& rhs )=delete;
        GameWindow& operator=( GameWindow&& rhs )=delete;
    private:
        GameWindowImp * imp_ptr;
    };
}

#endif