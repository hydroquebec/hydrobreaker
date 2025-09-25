#include "Game.hpp"
#include "States/Editor.hpp"
#include "States/MainMenu.hpp"
#include "States/OptionsMenu.hpp"
#include "States/Wallbreaker.hpp"
#include "Windows.h"
#include "stdlib.h"
#include <iostream>


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    Game& game = Game::getInstance();
    std::cout << __argv[0] << std::endl;
    game.init(__argv[0]);
    // Register game states
    game.addState("Editor", new Editor);
    game.addState("MainMenu", new MainMenu);
    game.addState("OptionsMenu", new OptionsMenu);
    game.addState("Wallbreaker", new Wallbreaker);

    game.setCurrentState("MainMenu");
    game.run();
    return 0;
}
