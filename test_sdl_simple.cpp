#include <iostream>
#include <SDL.h>

int main(int argc, char* argv[]) {
    std::cout << "Testing SDL2..." << std::endl;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("Test", 
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    std::cout << "SDL2 window created successfully!" << std::endl;
    
    SDL_Delay(2000); // Show window for 2 seconds
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::cout << "SDL2 test completed!" << std::endl;
    return 0;
}
