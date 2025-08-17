#pragma once

#include <cstdint>

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

        // Mouse (absolute delta per frame)
        float mouseDx = 0.0f;
        float mouseDy = 0.0f;
        uint32_t mouseButtons = 0; // bitmask of mouse buttons

        // Edges (press/release) for keys we care about
        bool pressedP = false;
        bool releasedP = false;
        bool pressedF1 = false;
        bool releasedF1 = false;
    };

    class Input
    {
    public:
        static InputSnapshot captureSnapshot();
    };
}


