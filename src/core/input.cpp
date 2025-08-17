// Copyright (c) Luster
#include "core/input.hpp"
#include <SDL3/SDL.h>

namespace luster
{
	static void getMouse(float& x, float& y, uint32_t& buttons)
	{
		float mx = 0.0f, my = 0.0f;
		buttons = SDL_GetMouseState(&mx, &my);
		x = mx; y = my;
	}

	InputSnapshot Input::captureSnapshot()
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
		uint32_t buttons = 0;
		getMouse(mx, my, buttons);
		snap.mouseButtons = buttons;
		static float lastx = mx, lasty = my;
		snap.mouseDx = mx - lastx;
		snap.mouseDy = my - lasty;
		lastx = mx; lasty = my;

		// Edge detection for keys (simple static previous state)
		static bool prevP = false, prevF1 = false;
		snap.pressedP = (ks[SDL_SCANCODE_P] && !prevP);
		snap.releasedP = (!ks[SDL_SCANCODE_P] && prevP);
		snap.pressedF1 = (ks[SDL_SCANCODE_F1] && !prevF1);
		snap.releasedF1 = (!ks[SDL_SCANCODE_F1] && prevF1);
		prevP = ks[SDL_SCANCODE_P];
		prevF1 = ks[SDL_SCANCODE_F1];
		return snap;
	}
}


