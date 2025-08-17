#pragma once

#include <cstdint>
#include <SDL3/SDL.h>

namespace luster
{
    struct InputSnapshot
    {
        // Keys
        bool keyW = false;
        bool keyA = false;
        bool keyS = false;
        bool keyD = false;
        bool keyQ = false;
        bool keyE = false;
        bool keyShift = false;
        bool keyCaps = false;
        bool keyEsc = false;
        bool keyP = false;
        bool keyF1 = false;

        // Mouse
        float mouseDx = 0.0f;
        float mouseDy = 0.0f;
        uint32_t mouseButtons = 0; // SDL_MouseButtonFlags
    };

    class Input
    {
    public:
        static InputSnapshot captureSnapshot()
        {
            InputSnapshot snap{};
            SDL_PumpEvents();
            const bool* ks = SDL_GetKeyboardState(nullptr);
            const SDL_Keymod mods = SDL_GetModState();
            snap.keyW = ks[SDL_SCANCODE_W];
            snap.keyA = ks[SDL_SCANCODE_A];
            snap.keyS = ks[SDL_SCANCODE_S];
            snap.keyD = ks[SDL_SCANCODE_D];
            snap.keyQ = ks[SDL_SCANCODE_Q];
            snap.keyE = ks[SDL_SCANCODE_E];
            snap.keyShift = ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT];
            snap.keyCaps = (mods & SDL_KMOD_CAPS) != 0 || ks[SDL_SCANCODE_CAPSLOCK];
            snap.keyEsc = ks[SDL_SCANCODE_ESCAPE];
            snap.keyP = ks[SDL_SCANCODE_P];
            snap.keyF1 = ks[SDL_SCANCODE_F1];

            float mx = 0.0f, my = 0.0f;
            snap.mouseButtons = SDL_GetMouseState(&mx, &my);
            static float lastx = mx, lasty = my;
            snap.mouseDx = mx - lastx;
            snap.mouseDy = my - lasty;
            lastx = mx; lasty = my;
            return snap;
        }
    };
}


