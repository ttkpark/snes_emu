#include <iostream>
#include <SDL.h>

int main() {
    std::cout << "Testing SDL2 Video..." << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Create window
    SDL_Window* window = SDL_CreateWindow("SNES Test Window", 
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         512, 448, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    std::cout << "SDL2 window created successfully!" << std::endl;
    std::cout << "Window should be visible now. Press any key to continue..." << std::endl;
    
    // Simple test pattern
    bool running = true;
    SDL_Event event;
    int frameCount = 0;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN) {
                running = false;
            }
        }
        
        // Clear screen with test pattern
        SDL_SetRenderDrawColor(renderer, 
                               (frameCount * 3) % 255, 
                               (frameCount * 5) % 255, 
                               (frameCount * 7) % 255, 
                               255);
        SDL_RenderClear(renderer);
        
        // Draw some test rectangles
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rect = {frameCount % 400, frameCount % 300, 50, 50};
        SDL_RenderFillRect(renderer, &rect);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
        
        frameCount++;
        if (frameCount % 60 == 0) {
            std::cout << "Frame " << frameCount << " rendered" << std::endl;
        }
    }
    
    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::cout << "SDL2 test completed successfully!" << std::endl;
    return 0;
}
