/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Game.h"
#include "../resource.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <SDL_mixer.h>
#include "State.h"
#include "Screen.h"
#include "Sound.h"
#include "Music.h"
#include "Language.h"
#include "Logger.h"
#include "../Interface/Cursor.h"
#include "../Interface/FpsCounter.h"
#include "../Interface/VirtualKeyboardState.h"
#include "../Interface/TextEdit.h"
#include "../Mod/Mod.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "Action.h"
#include "InteractiveSurface.h"
#include "Exception.h"
#include "Options.h"
#include "CrossPlatform.h"
#include "FileMap.h"
#include "Unicode.h"
#include "../Ufopaedia/UfopaediaStartState.h"
#include "../Menu/NotesState.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Menu/TestState.h"
#include "../fallthrough.h"

namespace OpenXcom
{

const double Game::VOLUME_GRADIENT = 10.0;

/**
 * Starts up all the SDL subsystems,
 * creates the display screen and sets up the cursor.
 * @param title Title of the game window.
 */
Game::Game(const std::string &title) : _screen(0), _cursor(0), _lang(0), _save(0), _mod(0), _quit(false), _init(false), _update(false),  _mouseActive(true), _timeUntilNextFrame(0),
	_ctrl(false), _alt(false), _shift(false), _rmb(false), _mmb(false), _scrollStep(1),
	_joystick(nullptr), _joystickAxisX(0), _joystickAxisY(0), _joystickHatState(SDL_HAT_CENTERED),
	_mouseButtons(0), _joystickMouseButtons(0),
	_joystickNavigation(false), _navigationAxisDown(false), _navigationWaitForNeutral(false), _keyboardButtonNavigation(false),
	_joystickHatKeys(SDL_HAT_CENTERED), _navigationRepeatHat(SDL_HAT_CENTERED),
	_joystickHatKeyState(nullptr), _navigationRepeatState(nullptr), _navigationRepeatSince(0), _navigationRepeating(false),
	_joystickCursorFracX(0.0f), _joystickCursorFracY(0.0f), _joystickLastTime(0)
{
	Options::reload = false;
	Options::mute = false;

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		Log(LOG_ERROR) << SDL_GetError();
		Log(LOG_WARNING) << "No video detected, quit.";
		throw Exception(SDL_GetError());
	}
	Log(LOG_INFO) << "SDL initialized successfully.";

	// Initialize SDL_mixer
	initAudio();

	// trap the mouse inside the window
	SDL_WM_GrabInput(Options::captureMouse);

	// Set the window icon
	CrossPlatform::setWindowIcon(IDI_ICON1, "openxcom.png");

	// Set the window caption
	SDL_WM_SetCaption(title.c_str(), 0);

	// Set up unicode
	SDL_EnableUNICODE(1);
	Unicode::getUtf8Locale();

	// Create display
	_screen = new Screen();

	// Create cursor
	_cursor = new Cursor(9, 13);

	// Create invisible hardware cursor to workaround bug with absolute positioning pointing devices
	SDL_ShowCursor(SDL_ENABLE);
	Uint8 cursor = 0;
	SDL_SetCursor(SDL_CreateCursor(&cursor, &cursor, 1,1,0,0));

	// Create fps counter
	_fpsCounter = new FpsCounter(15, 5, 0, 0);

	// Create blank language
	_lang = new Language();

	_timeOfLastFrame = 0;
	_mouseButtons = SDL_GetMouseState(nullptr, nullptr);

	// Initialize joystick subsystem
	if (Options::oxceJoystickEnabled)
	{
		Log(LOG_INFO) << "Joystick checking...";
		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
		{
			Log(LOG_WARNING) << "Could not initialize joystick subsystem: " << SDL_GetError();
		}
		else if (SDL_NumJoysticks() > 0)
		{
			_joystick = SDL_JoystickOpen(0);
			if (_joystick)
			{
				Log(LOG_INFO) << "Joystick opened: " << SDL_JoystickName(0);
				SDL_JoystickEventState(SDL_ENABLE);
			}
			else
			{
				Log(LOG_WARNING) << "Could not open joystick 0: " << SDL_GetError();
			}
		}
		else
		{
			Log(LOG_INFO) << "No joysticks found.";
		}
	}
}

/**
 * Deletes the display screen, cursor, states and shuts down all the SDL subsystems.
 */
Game::~Game()
{
	Sound::stop();
	Music::stop();

	for (auto* state : _states)
	{
		delete state;
	}

	SDL_FreeCursor(SDL_GetCursor());

	delete _cursor;
	delete _lang;
	delete _save;
	delete _mod;
	delete _screen;
	delete _fpsCounter;

	if (_joystick)
	{
		SDL_JoystickClose(_joystick);
		_joystick = nullptr;
	}

	Mix_CloseAudio();

	SDL_Quit();
}

void Game::resetButtonNavigationInput()
{
	_keyboardButtonNavigation = false;
	_navigationRepeatHat = SDL_HAT_CENTERED;
	_navigationRepeatState = nullptr;
	_navigationRepeating = false;
	_navigationWaitForNeutral = _joystickHatState != SDL_HAT_CENTERED;
	const Uint8 masks[] = { SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT };
	const SDLKey keys[] = { SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT };
	for (int i = 0; i < 4; ++i)
	{
		if (_joystickHatKeys & masks[i])
		{
			SDL_Event release = {};
			release.type = SDL_USEREVENT;
			release.user.code = CONTROLLER_HAT_RELEASE;
			release.user.data1 = _joystickHatKeyState;
			release.user.data2 = reinterpret_cast<void*>(static_cast<intptr_t>(keys[i]));
			SDL_PushEvent(&release);
		}
	}
	_joystickHatKeys = SDL_HAT_CENTERED;
	_joystickHatKeyState = nullptr;
}

void Game::toggleButtonNavigation()
{
	resetButtonNavigationInput();
	_joystickNavigation = !_joystickNavigation;
	if (_joystickNavigation)
	{
		if (!_states.back()->getNavigationButton())
			_states.back()->navigateButtons(0, 0);
	}
	else
		_states.back()->clearButtonNavigation();
}

bool Game::handleButtonNavigation(SDL_Event &event)
{
	if (event.type == SDL_KEYUP)
		return _navigationHeldKeys.erase(event.key.keysym.sym) != 0;
	if (event.type != SDL_KEYDOWN)
		return false;
	const SDLKey key = event.key.keysym.sym;
	const bool direction = key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN;
	// Activation and navigation cancellation require a fresh press, even if
	// an editor enabled SDL repeat. Cancel can itself be remapped to Tab.
	if (((key != SDLK_TAB && !direction) || key == Options::keyCancel) && _navigationHeldKeys.count(key))
		return true;
	State *state = _states.back();
	InteractiveSurface *selected = state->getNavigationButton();
	TextEdit *editor = dynamic_cast<TextEdit*>(selected);
	const bool alternate = selected && !editor && (key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
		(event.key.keysym.mod & KMOD_CTRL);
	if (dynamic_cast<VirtualKeyboardState*>(state) ||
		(event.key.keysym.mod & (KMOD_ALT | KMOD_META)) || ((event.key.keysym.mod & KMOD_CTRL) && !alternate))
		return false;
	if (key == Options::keyCancel)
	{
		if (!_joystickNavigation && !selected && !state->isNavigationEditing())
			return false;
		resetButtonNavigationInput();
		if (state->cancelNavigationControl())
			_keyboardButtonNavigation = true;
		else
		{
			_joystickNavigation = false;
			state->clearButtonNavigation();
		}
		_navigationHeldKeys.insert(key);
		return true;
	}
	if (!_mouseActive || !_cursor->getVisible())
		return false;
	if (key == SDLK_TAB && Options::oxceKeyboardButtonNavigation)
	{
		if (_states.back()->navigateButtons(0, 0, (event.key.keysym.mod & KMOD_SHIFT) != 0))
		{
			resetButtonNavigationInput();
			_keyboardButtonNavigation = true;
			_navigationHeldKeys.insert(key);
			return true;
		}
	}
	else if (editor && state->isNavigationEditing())
	{
		// Typing, Space and native editor shortcuts retain their original
		// meaning; leaving the editor is handled above without sending Escape.
		return false;
	}
	else if (direction && selected && (_keyboardButtonNavigation || _joystickNavigation))
	{
		_navigationHeldKeys.insert(key);
		state->navigateButtons(key == SDLK_LEFT ? -1 : key == SDLK_RIGHT ? 1 : 0,
			key == SDLK_UP ? -1 : key == SDLK_DOWN ? 1 : 0);
		return true;
	}
	else if (_states.back()->getNavigationButton() &&
		(_keyboardButtonNavigation || !_states.back()->getFocusedTextEdit()) &&
		(key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE))
	{
		_navigationHeldKeys.insert(key);
		const Uint8 button = (event.key.keysym.mod & KMOD_CTRL) ? SDL_BUTTON_MIDDLE :
			(event.key.keysym.mod & KMOD_SHIFT) ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
		const bool editing = state->isNavigationEditing();
		state->activateNavigationButton(button);
		if (_states.back() == state && editing != state->isNavigationEditing())
		{
			resetButtonNavigationInput();
			_keyboardButtonNavigation = true;
		}
		return true;
	}
	else if (key == SDLK_ESCAPE || event.key.keysym.unicode >= 32)
	{
		_keyboardButtonNavigation = false;
		if (!_joystickNavigation)
			_states.back()->clearButtonNavigation();
	}
	return false;
}

void Game::navigateJoystickButtons(Uint8 hat)
{
	if (!_mouseActive || !_cursor->getVisible())
		return;
	if (hat != SDL_HAT_CENTERED)
		_keyboardButtonNavigation = true;
	State *state = _states.back();
	State *owner = state->getNavigationState();
	const Uint8 masks[] = {SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT};
	const int dx[] = {0, 0, -1, 1}, dy[] = {-1, 1, 0, 0};
	for (int i = 0; i < 4; ++i)
	{
		if (hat & masks[i])
		{
			state->navigateButtons(dx[i], dy[i]);
			// A control callback can open a screen or finish an interception.
			// The rest of this diagonal belongs to the original control only.
			if (getState() != state || state->getNavigationState() != owner)
			{
				resetButtonNavigationInput();
				return;
			}
		}
	}
}

void Game::repeatButtonNavigation(Uint8 liveHat)
{
	const Uint8 focus = SDL_APPINPUTFOCUS | SDL_APPACTIVE;
	if (_navigationRepeatState && _navigationRepeatState != _states.back()->getNavigationState())
		resetButtonNavigationInput();
	if (!_joystickNavigation || !Options::oxceJoystickEnabled || _navigationWaitForNeutral ||
		_navigationRepeatState != _states.back()->getNavigationState() || (SDL_GetAppState() & focus) != focus ||
		dynamic_cast<VirtualKeyboardState*>(_states.back()) || liveHat != _navigationRepeatHat)
	{
		_navigationRepeatHat = SDL_HAT_CENTERED;
		_navigationRepeatState = nullptr;
		return;
	}
	if (liveHat == SDL_HAT_CENTERED)
		return;
	const Uint32 now = SDL_GetTicks();
	if ((Uint32)(now - _navigationRepeatSince) >= (_navigationRepeating ? 100u : 400u))
	{
		_navigationRepeatSince = now;
		_navigationRepeating = true;
		navigateJoystickButtons(liveHat);
	}
}

/**
 * Translates controller buttons before normal event dispatch, so mouse and
 * controller input share the cursor, UI handlers and held-button state.
 * @return True if the event should be dispatched, false if it was consumed.
 */
bool Game::convertInputEvent(SDL_Event &event)
{
	switch (event.type)
	{
	case SDL_ACTIVEEVENT:
		if (!event.active.gain && (event.active.state & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)))
		{
			resetButtonNavigationInput();
			if (auto *keyboard = dynamic_cast<VirtualKeyboardState*>(_states.back()))
				keyboard->armDpadRepeat(SDL_HAT_CENTERED);
		}
		break;
	case SDL_USEREVENT:
		if (event.user.code == NAVIGATION_KEY_RELEASE_EVENT)
		{
			_navigationHeldKeys.erase(static_cast<SDLKey>(reinterpret_cast<intptr_t>(event.user.data2)));
			return false;
		}
		if (event.user.code == CONTROLLER_HAT_PRESS || event.user.code == CONTROLLER_HAT_RELEASE)
		{
			State *owner = static_cast<State*>(event.user.data1);
			const bool pressed = event.user.code == CONTROLLER_HAT_PRESS;
			if (std::find(_states.begin(), _states.end(), owner) == _states.end() ||
				(pressed && (owner != _states.back() || _navigationWaitForNeutral ||
				!Options::oxceJoystickEnabled ||
				(SDL_GetAppState() & (SDL_APPINPUTFOCUS | SDL_APPACTIVE)) != (SDL_APPINPUTFOCUS | SDL_APPACTIVE) ||
				(_joystickNavigation && !dynamic_cast<VirtualKeyboardState*>(owner)))))
				return false;
			SDL_Event key = {};
			key.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
			key.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
			key.key.keysym.sym = static_cast<SDLKey>(reinterpret_cast<intptr_t>(event.user.data2));
			if (owner == _states.back())
			{
				event = key;
				return true;
			}
			// A release must reach the original camera even if a dialog opened.
			Action action(&key, _screen->getXScale(), _screen->getYScale(), _screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
			owner->handle(&action);
			return false;
		}
		if (event.user.code == CONTROLLER_DELETE_EVENT)
		{
			if (event.user.data1 == _states.back() &&
				(Options::keyboardMode == KEYBOARD_ON || Options::keyboardMode == KEYBOARD_VIRTUAL))
			{
				TextEdit *editor = _states.back()->getFocusedTextEdit();
				if (editor && editor == event.user.data2)
					editor->typeVirtualKey(SDLK_BACKSPACE, 0);
			}
			return false;
		}
		if (event.user.code == CONTROLLER_CONFIRM_EVENT || event.user.code == CONTROLLER_CANCEL_EVENT)
		{
			// Do not let a queued confirmation act on a different dialog.
			if (event.user.data1 == _states.back())
			{
				bool confirm = event.user.code == CONTROLLER_CONFIRM_EVENT;
				SDL_Event keyEvent = {};
				keyEvent.type = SDL_KEYDOWN;
				keyEvent.key.state = SDL_PRESSED;
				keyEvent.key.keysym.sym = confirm ? Options::keyOk : Options::keyCancel;
				keyEvent.key.keysym.mod = KMOD_NONE;
				Action action(&keyEvent, _screen->getXScale(), _screen->getYScale(), _screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
				_states.back()->handleControllerButton(confirm, &action);
			}
			return false;
		}
		break;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		return !handleButtonNavigation(event);
	case SDL_MOUSEMOTION:
		if (event.motion.xrel != 0 || event.motion.yrel != 0)
		{
			_keyboardButtonNavigation = false;
			if (!_joystickNavigation)
				_states.back()->clearButtonNavigation();
		}
		_mouseButtons = event.motion.state;
		event.motion.state |= _joystickMouseButtons;
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	{
		if (event.type == SDL_MOUSEBUTTONDOWN)
		{
			_keyboardButtonNavigation = false;
			if (!_joystickNavigation)
				_states.back()->clearButtonNavigation();
		}
		Uint8 mask = SDL_BUTTON(event.button.button);
		if (event.type == SDL_MOUSEBUTTONDOWN)
			_mouseButtons |= mask;
		else
			_mouseButtons &= ~mask;
		// The shared button stays pressed until both input sources release it.
		return (_joystickMouseButtons & mask) == 0;
	}
	case SDL_JOYAXISMOTION:
		if (_joystick && event.jaxis.which == SDL_JoystickIndex(_joystick))
		{
			if (event.jaxis.axis == 0)
				_joystickAxisX = event.jaxis.value;
			else if (event.jaxis.axis == 1)
				_joystickAxisY = event.jaxis.value;
			else if (event.jaxis.axis == Options::oxceJoystickAxisNavigation)
			{
				if (event.jaxis.value < 8000)
					_navigationAxisDown = false;
				else if (event.jaxis.value > 16000 && !_navigationAxisDown)
				{
					_navigationAxisDown = true;
					const Uint8 focus = SDL_APPINPUTFOCUS | SDL_APPACTIVE;
					if (Options::oxceJoystickEnabled && (SDL_GetAppState() & focus) == focus &&
						!dynamic_cast<VirtualKeyboardState*>(_states.back()))
						toggleButtonNavigation();
				}
			}
		}
		return false;
	case SDL_JOYHATMOTION:
		if (_joystick && event.jhat.which == SDL_JoystickIndex(_joystick) && event.jhat.hat == 0)
		{
			struct HatKey { Uint8 mask; SDLKey key; };
			static const HatKey hatKeys[4] =
			{
				{ SDL_HAT_UP,    SDLK_UP    },
				{ SDL_HAT_DOWN,  SDLK_DOWN  },
				{ SDL_HAT_LEFT,  SDLK_LEFT  },
				{ SDL_HAT_RIGHT, SDLK_RIGHT },
			};
			Uint8 oldHat = _joystickHatState;
			_joystickHatState = event.jhat.value;
			if (_joystickHatState == SDL_HAT_CENTERED)
				_navigationWaitForNeutral = false;
			const bool keyboard = dynamic_cast<VirtualKeyboardState*>(_states.back()) != nullptr;
			if (_joystickNavigation && !keyboard)
			{
				const Uint8 focus = SDL_APPINPUTFOCUS | SDL_APPACTIVE;
				if (oldHat != _joystickHatState && !_navigationWaitForNeutral &&
					Options::oxceJoystickEnabled && (SDL_GetAppState() & focus) == focus)
				{
					navigateJoystickButtons(_joystickHatState & ~oldHat);
					if (_navigationWaitForNeutral)
						return false;
					_navigationRepeatHat = _joystickHatState;
					_navigationRepeatState = _states.back()->getNavigationState();
					_navigationRepeatSince = SDL_GetTicks();
					_navigationRepeating = false;
				}
				return false;
			}
			for (const auto &hatKey : hatKeys)
			{
				bool wasDown = (_joystickHatKeys & hatKey.mask) != 0;
				bool isDown = !_navigationWaitForNeutral && (_joystickHatState & hatKey.mask) != 0;
				if (wasDown != isDown)
				{
					if (isDown && _joystickHatKeys == SDL_HAT_CENTERED)
						_joystickHatKeyState = _states.back();
					SDL_Event keyEvent = {};
					keyEvent.type = SDL_USEREVENT;
					keyEvent.user.code = isDown ? CONTROLLER_HAT_PRESS : CONTROLLER_HAT_RELEASE;
					keyEvent.user.data1 = _joystickHatKeyState;
					keyEvent.user.data2 = reinterpret_cast<void*>(static_cast<intptr_t>(hatKey.key));
					SDL_PushEvent(&keyEvent);
					if (isDown) _joystickHatKeys |= hatKey.mask;
					else _joystickHatKeys &= ~hatKey.mask;
				}
			}
			if (oldHat != _joystickHatState)
			{
				if (auto *keyboard = dynamic_cast<VirtualKeyboardState*>(_states.back()))
					keyboard->armDpadRepeat(_navigationWaitForNeutral ? SDL_HAT_CENTERED : _joystickHatState);
			}
		}
		return false;
	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
	{
		if (!_joystick || event.jbutton.which != SDL_JoystickIndex(_joystick))
			return false;
		bool pressed = event.type == SDL_JOYBUTTONDOWN;
		Uint8 button = event.jbutton.button;
		Uint8 oldMouseButtons = _joystickMouseButtons;
		JoystickButtonBinding binding;
		if (pressed)
		{
			// A held button keeps its original action even if a dialog closes or
			// the configuration changes before its release.
			if (_joystickButtonBindings.find(button) != _joystickButtonBindings.end())
				return false;
			bool keyboard = dynamic_cast<VirtualKeyboardState*>(_states.back()) != nullptr;
			InteractiveSurface *selected = !keyboard ? _states.back()->getNavigationButton() : nullptr;
			const bool navigation = !keyboard && (_joystickNavigation || _keyboardButtonNavigation);
			const bool alternate = navigation && selected && !dynamic_cast<TextEdit*>(selected);
			if (!keyboard && button == Options::oxceJoystickButtonNavigation)
				binding.command = CONTROLLER_NAVIGATION_TOGGLE;
			else if (button == Options::oxceJoystickButtonKeyboard && Options::keyboardMode == KEYBOARD_VIRTUAL)
			{
				if (alternate)
					binding.command = CONTROLLER_NAVIGATION_TERTIARY;
				else
				{
					if (navigation && selected && !_states.back()->isNavigationEditing())
						_states.back()->activateNavigationButton();
					binding.command = VirtualKeyboardState::TOGGLE_EVENT;
				}
			}
			else if (button == Options::oxceJoystickButtonLeftClick)
			{
				if (navigation)
					binding.command = CONTROLLER_NAVIGATION_NEXT;
				else
					binding.mouseButton = SDL_BUTTON_LEFT;
			}
			else if (button == Options::oxceJoystickButtonRightClick)
			{
				if (navigation && selected)
					binding.command = CONTROLLER_NAVIGATION_SECONDARY;
				else
					binding.mouseButton = SDL_BUTTON_RIGHT;
			}
			else if (button == Options::oxceJoystickButtonOk)
			{
				if (keyboard)
					binding.key = SDLK_RETURN;
				else if (navigation)
					binding.command = CONTROLLER_NAVIGATION_ACTIVATE;
				else
				{
					int x, y;
					SDL_GetMouseState(&x, &y);
					SDL_Event pointerEvent = {};
					Action pointer(&pointerEvent, _screen->getXScale(), _screen->getYScale(), _screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
					pointer.setMouseAction(x, y, 0, 0);
					if (_states.back()->getControllerButton(true) &&
						!_states.back()->isMouseTarget(pointer.getAbsoluteXMouse(), pointer.getAbsoluteYMouse(), SDL_BUTTON_LEFT))
					{
						binding.command = CONTROLLER_CONFIRM_EVENT;
						binding.state = _states.back();
					}
					else
						binding.mouseButton = SDL_BUTTON_LEFT;
				}
			}
			else if (button == Options::oxceJoystickButtonCancel)
			{
				if (keyboard)
					binding.key = SDLK_ESCAPE;
				else if (_joystickNavigation || selected || _states.back()->isNavigationEditing())
				{
					resetButtonNavigationInput();
					if (_states.back()->cancelNavigationControl())
						_keyboardButtonNavigation = true;
					else
					{
						_joystickNavigation = false;
						_states.back()->clearButtonNavigation();
					}
					// Keep the empty binding latched until release: this press
					// only exits navigation, without cancelling or right-clicking.
				}
				else if (_states.back()->getControllerButton(false))
				{
					binding.command = CONTROLLER_CANCEL_EVENT;
					binding.state = _states.back();
				}
				else
					binding.mouseButton = SDL_BUTTON_RIGHT;
			}
			else if (button == Options::oxceJoystickButtonDelete)
			{
				if (keyboard)
					binding.key = SDLK_BACKSPACE;
				else if (alternate)
					binding.command = CONTROLLER_NAVIGATION_SECONDARY;
				else if ((Options::keyboardMode == KEYBOARD_ON || Options::keyboardMode == KEYBOARD_VIRTUAL) &&
					(binding.editor = _states.back()->getFocusedTextEdit()) != nullptr)
				{
					binding.command = CONTROLLER_DELETE_EVENT;
					binding.state = _states.back();
				}
				else
					binding.key = SDLK_SPACE;
			}
			else if (button == Options::oxceJoystickButtonKeyboard)
			{
				if (alternate)
					binding.command = CONTROLLER_NAVIGATION_TERTIARY;
				else
					binding.key = Options::keyOk;
			}
			else if (button >= 6 && button <= 9)
				binding.key = keyboard ? SDLK_ESCAPE : Options::keyCancel;
			_joystickButtonBindings[button] = binding;
		}
		else
		{
			auto held = _joystickButtonBindings.find(button);
			if (held == _joystickButtonBindings.end())
				return false;
			binding = held->second;
			_joystickButtonBindings.erase(held);
		}

		if (binding.mouseButton != 0)
		{
			if (pressed)
			{
				_keyboardButtonNavigation = false;
				if (!_joystickNavigation)
					_states.back()->clearButtonNavigation();
			}
			// Multiple controller buttons may hold the same logical mouse button.
			_joystickMouseButtons = 0;
			for (const auto &held : _joystickButtonBindings)
			{
				if (held.second.mouseButton != 0)
					_joystickMouseButtons |= SDL_BUTTON(held.second.mouseButton);
			}
			Uint8 mask = SDL_BUTTON(binding.mouseButton);
			bool wasDown = ((_mouseButtons | oldMouseButtons) & mask) != 0;
			bool isDown = ((_mouseButtons | _joystickMouseButtons) & mask) != 0;
			if (wasDown == isDown)
				return false;

			int x, y;
			SDL_GetMouseState(&x, &y);
			event = SDL_Event{};
			event.type = isDown ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
			event.button.button = binding.mouseButton;
			event.button.state = isDown ? SDL_PRESSED : SDL_RELEASED;
			event.button.x = (Uint16)x;
			event.button.y = (Uint16)y;
			return true;
		}
		if (binding.command >= 0)
		{
			if (binding.command == CONTROLLER_NAVIGATION_TOGGLE || binding.command == CONTROLLER_NAVIGATION_ACTIVATE ||
				binding.command == CONTROLLER_NAVIGATION_SECONDARY || binding.command == CONTROLLER_NAVIGATION_TERTIARY ||
				binding.command == CONTROLLER_NAVIGATION_NEXT)
			{
				const Uint8 focus = SDL_APPINPUTFOCUS | SDL_APPACTIVE;
				if (pressed && Options::oxceJoystickEnabled && (SDL_GetAppState() & focus) == focus)
				{
					if (binding.command == CONTROLLER_NAVIGATION_TOGGLE)
						toggleButtonNavigation();
					else if (_mouseActive && _cursor->getVisible())
					{
						State *state = _states.back();
						const bool editing = state->isNavigationEditing();
						if (binding.command == CONTROLLER_NAVIGATION_NEXT)
							_states.back()->navigateButtons(0, 0);
						else
							_states.back()->activateNavigationButton(binding.command == CONTROLLER_NAVIGATION_SECONDARY ? SDL_BUTTON_RIGHT :
								binding.command == CONTROLLER_NAVIGATION_TERTIARY ? SDL_BUTTON_MIDDLE : SDL_BUTTON_LEFT);
						if (_states.back() == state && (binding.command == CONTROLLER_NAVIGATION_NEXT ||
							editing != state->isNavigationEditing()))
						{
							resetButtonNavigationInput();
							_keyboardButtonNavigation = true;
						}
					}
				}
				return false;
			}
			if (pressed)
			{
				SDL_Event command = {};
				command.type = SDL_USEREVENT;
				command.user.code = binding.command;
				command.user.data1 = binding.state;
				command.user.data2 = binding.editor;
				SDL_PushEvent(&command);
			}
			return false;
		}
		if (binding.key != SDLK_UNKNOWN)
		{
			event = SDL_Event{};
			event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
			event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
			event.key.keysym.sym = binding.key;
			event.key.keysym.mod = KMOD_NONE;
			// Keep these ordered with the arrow events generated by the D-pad.
			SDL_PushEvent(&event);
		}
		return false;
	}
	default:
		break;
	}
	return true;
}

/**
 * The state machine takes care of passing all the events from SDL to the
 * active state, running any code within and blitting all the states and
 * cursor to the screen. This is run indefinitely until the game quits.
 */
void Game::run()
{
	enum ApplicationState { RUNNING = 0, SLOWED = 1, PAUSED = 2 } runningState = RUNNING;
	static const ApplicationState kbFocusRun[4] = { RUNNING, RUNNING, SLOWED, PAUSED };
	static const ApplicationState stateRun[4] = { SLOWED, PAUSED, PAUSED, PAUSED };
	// this will avoid processing SDL's resize event on startup, workaround for the heap allocation error it causes.
	bool startupEvent = Options::allowResize;
	Uint32 lastMouseMoveEvent = 0;
	Sint16 xrel = 0;
	Sint16 yrel = 0;

	while (!_quit)
	{
		// Clean up states
		while (!_deleted.empty())
		{
			delete _deleted.back();
			_deleted.pop_back();
		}

		// Initialize active state
		if (!_init)
		{
			resetButtonNavigationInput();
			_init = true;
			_states.back()->init();
			if (auto *keyboard = dynamic_cast<VirtualKeyboardState*>(_states.back()))
				keyboard->armDpadRepeat(SDL_HAT_CENTERED);
			else if (_joystickNavigation && !_states.back()->getNavigationButton())
				_states.back()->navigateButtons(0, 0);

			// Unpress buttons
			_states.back()->resetAll();

			// Refresh mouse position
			SDL_Event ev;
			SDL_memset(&ev, 0, sizeof(ev));
			int x, y;
			SDL_GetMouseState(&x, &y);
			ev.type = SDL_MOUSEMOTION;
			ev.motion.state = getMouseButtonState();
			ev.motion.x = x;
			ev.motion.y = y;
			Action action = Action(&ev, _screen->getXScale(), _screen->getYScale(), _screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
			_states.back()->handle(&action);
		}

		// Pump devices once per frame. SDL 1.2's SDL_PollEvent pumps again on
		// every call, so noisy joystick axes can keep the queue nonempty and
		// prevent logic and cursor rendering from running.
		SDL_PumpEvents();
		// Video players can consume KEYUP outside the main loop. Recover a
		// navigation key release in queue order, without replaying activation.
		if (!_navigationHeldKeys.empty())
		{
			int keyCount = 0;
			const Uint8 *keys = SDL_GetKeyState(&keyCount);
			for (SDLKey key : _navigationHeldKeys)
			{
				if (key >= 0 && key < keyCount && keys[key] == SDL_RELEASED)
				{
					SDL_Event release = {};
					release.type = SDL_USEREVENT;
					release.user.code = NAVIGATION_KEY_RELEASE_EVENT;
					release.user.data2 = reinterpret_cast<void*>(static_cast<intptr_t>(key));
					SDL_PushEvent(&release);
				}
			}
		}
		if (_joystick)
		{
			// Loading, video playback and focus changes can discard release
			// events. Recover them through the same latched input path.
			for (const auto &held : _joystickButtonBindings)
			{
				if (SDL_JoystickGetButton(_joystick, held.first) == SDL_RELEASED)
				{
					SDL_Event release = {};
					release.type = SDL_JOYBUTTONUP;
					release.jbutton.which = (Uint8)SDL_JoystickIndex(_joystick);
					release.jbutton.button = held.first;
					release.jbutton.state = SDL_RELEASED;
					SDL_PushEvent(&release);
				}
			}
		}

		// Include generated key events, but always leave time to redraw.
		unsigned int eventsProcessed = 0;
		while (eventsProcessed++ < 256 && SDL_PeepEvents(&_event, 1, SDL_GETEVENT, SDL_ALLEVENTS) > 0)
		{
			Log(LOG_DEBUG) << "SDL event type is " << static_cast<int>(_event.type);

			if (CrossPlatform::isQuitShortcut(_event))
				_event.type = SDL_QUIT;
			if (!convertInputEvent(_event))
			{
				if (!_init)
					break;
				continue;
			}
			switch (_event.type)
			{
				case SDL_QUIT:
					quit();
					break;
				case SDL_ACTIVEEVENT:
					// An event other than SDL_APPMOUSEFOCUS change happened.
					if (reinterpret_cast<SDL_ActiveEvent*>(&_event)->state & ~SDL_APPMOUSEFOCUS)
					{
						Uint8 currentState = SDL_GetAppState();
						// Game is minimized
						if (!(currentState & SDL_APPACTIVE))
						{
							runningState = stateRun[Options::pauseMode];
							if (Options::backgroundMute)
							{
								setVolume(0, 0, 0);
							}
						}
						// Game is not minimized but has no keyboard focus.
						else if (!(currentState & SDL_APPINPUTFOCUS))
						{
							runningState = kbFocusRun[Options::pauseMode];
							if (Options::backgroundMute)
							{
								setVolume(0, 0, 0);
							}
						}
						// Game has keyboard focus.
						else
						{
							runningState = RUNNING;
							if (Options::backgroundMute)
							{
								setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
							}
						}
					}
					break;
				case SDL_VIDEORESIZE:
					if (Options::allowResize)
					{
						if (!startupEvent)
						{
							Options::newDisplayWidth = Options::displayWidth = std::max(Screen::ORIGINAL_WIDTH, _event.resize.w);
							Options::newDisplayHeight = Options::displayHeight = std::max(Screen::ORIGINAL_HEIGHT, _event.resize.h);
							int dX = 0, dY = 0;
							Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, false);
							Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, false);
							for (auto* state : _states)
							{
								state->resize(dX, dY);
							}
							_screen->resetDisplay();
						}
						else
						{
							startupEvent = false;
						}
					}
					break;
				case SDL_MOUSEMOTION:
					if (Options::oxceThrottleMouseMoveEvent > 0)
					{
						Uint32 last = SDL_GetTicks();
						if (0 == lastMouseMoveEvent)
						{
							lastMouseMoveEvent = last;
						}
						if (last - lastMouseMoveEvent < (Uint32)Options::oxceThrottleMouseMoveEvent)
						{
							xrel += _event.motion.xrel;
							yrel += _event.motion.yrel;
							continue;
						}
						lastMouseMoveEvent = 0;
						_event.motion.xrel += std::exchange(xrel, 0);
						_event.motion.yrel += std::exchange(yrel, 0);
					}
					FALLTHROUGH;
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
					// Skip mouse events if they're disabled
					if (!_mouseActive) continue;
					// re-gain focus on mouse-over or keypress.
					runningState = RUNNING;
					// Go on, feed the event to others
					FALLTHROUGH;
				default:
					Action action = Action(&_event, _screen->getXScale(), _screen->getYScale(), _screen->getCursorTopBlackBand(), _screen->getCursorLeftBlackBand());
					_screen->handle(&action);
					_cursor->handle(&action);
					_fpsCounter->handle(&action);
					if (action.getDetails()->type == SDL_KEYDOWN)
					{
						// "ctrl-g" grab input
						if (action.getDetails()->key.keysym.sym == SDLK_g && isCtrlPressed())
						{
							Options::captureMouse = (SDL_GrabMode)(!Options::captureMouse);
							SDL_WM_GrabInput(Options::captureMouse);
						}
						// "ctrl-n" notes UI
						else if (action.getDetails()->key.keysym.sym == SDLK_n && isCtrlPressed() && !isAltPressed())
						{
							if (_save && !containsNotesState())
							{
								if (_save->getSavedBattle())
								{
									if (!_save->getSavedBattle()->isBattlescapeStateBusy())
									{
										pushState(new NotesState(OPT_BATTLESCAPE));
									}
								}
								else
								{
									pushState(new NotesState(OPT_GEOSCAPE));
								}
							}
						}
						else if (Options::debug)
						{
							if (action.getDetails()->key.keysym.sym == SDLK_t && isCtrlPressed())
							{
								pushState(new TestState);
							}
							// "ctrl-u" debug UI
							else if (action.getDetails()->key.keysym.sym == SDLK_u && isCtrlPressed())
							{
								Options::debugUi = !Options::debugUi;
								_states.back()->redrawText();
							}
						}
					}
					_states.back()->handle(&action);
					break;
			}
			if (!_init)
			{
				// States stack was changed, break the loop so new state
				// can be initialized before processing new events
				break;
			}
		}

		// SDL's keyboard repeat does not apply to queued D-pad arrow events.
		// Poll the device as well, so a missed release cannot keep navigation going.
		auto *keyboard = dynamic_cast<VirtualKeyboardState*>(_states.back());
		if (keyboard || _joystickNavigation || _navigationWaitForNeutral)
		{
			Uint8 hat = SDL_HAT_CENTERED;
			if (_joystick && SDL_JoystickNumHats(_joystick) > 0)
			{
				hat = SDL_JoystickGetHat(_joystick, 0);
				if (hat == SDL_HAT_CENTERED && _joystickHatState != SDL_HAT_CENTERED)
				{
					// Recover a missed release in event order. Resetting the cached
					// mask here could discard a press still in the bounded queue.
					SDL_Event release = {};
					release.type = SDL_JOYHATMOTION;
					release.jhat.which = (Uint8)SDL_JoystickIndex(_joystick);
					release.jhat.hat = 0;
					release.jhat.value = SDL_HAT_CENTERED;
					SDL_PushEvent(&release);
				}
			}
			const Uint8 liveHat = _init && runningState != PAUSED ? hat : SDL_HAT_CENTERED;
			if (keyboard)
				keyboard->repeatDpad(liveHat);
			else
				repeatButtonNavigation(liveHat);
		}
		if (_joystick && _navigationAxisDown && Options::oxceJoystickAxisNavigation >= 2 &&
			Options::oxceJoystickAxisNavigation < SDL_JoystickNumAxes(_joystick) &&
			SDL_JoystickGetAxis(_joystick, Options::oxceJoystickAxisNavigation) < 8000)
		{
			// Keep release recovery behind queued positive samples from the
			// same press, otherwise a backlog could toggle the mode twice.
			SDL_Event release = {};
			release.type = SDL_JOYAXISMOTION;
			release.jaxis.which = (Uint8)SDL_JoystickIndex(_joystick);
			release.jaxis.axis = (Uint8)Options::oxceJoystickAxisNavigation;
			release.jaxis.value = SDL_JoystickGetAxis(_joystick, Options::oxceJoystickAxisNavigation);
			SDL_PushEvent(&release);
		}

		// Process rendering
		if (runningState != PAUSED)
		{
			// Use the current device state even if another state consumed an
			// axis event (for example while leaving a video or loading a game).
			if (_joystick)
			{
				_joystickAxisX = SDL_JoystickGetAxis(_joystick, 0);
				_joystickAxisY = SDL_JoystickGetAxis(_joystick, 1);
			}
			// Move cursor based on joystick left-stick axis values
			if (_joystick && _mouseActive && (SDL_GetAppState() & SDL_APPINPUTFOCUS) &&
			    (std::abs(_joystickAxisX) > Options::oxceJoystickDeadZone ||
			     std::abs(_joystickAxisY) > Options::oxceJoystickDeadZone))
			{
				Uint32 now = SDL_GetTicks();
				if (_joystickLastTime > 0)
				{
					float dt = (now - _joystickLastTime) / 1000.0f;
					if (dt > 0.1f) dt = 0.1f; // cap to avoid large jumps after pauses

					float axisX = (std::abs(_joystickAxisX) > Options::oxceJoystickDeadZone)
					              ? (float)_joystickAxisX / 32767.0f : 0.0f;
					float axisY = (std::abs(_joystickAxisY) > Options::oxceJoystickDeadZone)
					              ? (float)_joystickAxisY / 32767.0f : 0.0f;

					_joystickCursorFracX += axisX * Options::oxceJoystickCursorSpeed * dt;
					_joystickCursorFracY += axisY * Options::oxceJoystickCursorSpeed * dt;

					int dx = (int)_joystickCursorFracX;
					int dy = (int)_joystickCursorFracY;
					_joystickCursorFracX -= (float)dx;
					_joystickCursorFracY -= (float)dy;

					if (dx != 0 || dy != 0)
					{
						int mx, my;
						SDL_GetMouseState(&mx, &my);
						int targetX = std::max(0, std::min(mx + dx, _screen->getWidth() - 1));
						int targetY = std::max(0, std::min(my + dy, _screen->getHeight() - 1));
						if (targetX != mx || targetY != my)
							SDL_WarpMouse((Uint16)targetX, (Uint16)targetY);
					}
				}
				_joystickLastTime = now;
			}
			else
			{
				_joystickLastTime = 0;
				_joystickCursorFracX = 0.0f;
				_joystickCursorFracY = 0.0f;
			}

			// Process logic
			_states.back()->think();
			_fpsCounter->think();
			if (Options::FPS > 0 && !(Options::useOpenGL && Options::vSyncForOpenGL))
			{
				// Update our FPS delay time based on the time of the last draw.
				int fps = SDL_GetAppState() & SDL_APPINPUTFOCUS ? Options::FPS : Options::FPSInactive;

				_timeUntilNextFrame = (1000.0f / fps) - (SDL_GetTicks() - _timeOfLastFrame);
			}
			else
			{
				_timeUntilNextFrame = 0;
			}

			if (_init && _timeUntilNextFrame <= 0)
			{
				// make a note of when this frame update occurred.
				_timeOfLastFrame = SDL_GetTicks();
				_fpsCounter->addFrame();
				_screen->clear();
				std::list<State*>::iterator i = _states.end();
				do
				{
					--i;
				}
				while (i != _states.begin() && !(*i)->isScreen());

				for (; i != _states.end(); ++i)
				{
					(*i)->blit();
				}
				_states.back()->blitButtonNavigation();
				_fpsCounter->blit(_screen->getSurface());
				_cursor->blit(_screen->getSurface());
				_screen->flip();
			}
		}

		// Save on CPU
		switch (runningState)
		{
			case RUNNING:
				SDL_Delay(1); //Save CPU from going 100%
				break;
			case SLOWED: case PAUSED:
				SDL_Delay(100); break; //More slowing down.
		}
	}

	Options::save();
}

/**
 * Stops the state machine and the game is shut down.
 */
void Game::quit()
{
	// Hard-learned lesson: there's a billion+ situations, where this causes a corrupted save and subsequent crashes. It's not worth it!
#if 0
	// Always save ironman
	if (_save != 0 && _save->isIronman() && !_save->getName().empty())
	{
		std::string filename = CrossPlatform::sanitizeFilename(_save->getName()) + ".sav";
		_save->save(filename, _mod);
	}
#endif
	_quit = true;
}

/**
 * Changes the audio volume of the music and
 * sound effect channels.
 * @param sound Sound volume, from 0 to MIX_MAX_VOLUME.
 * @param music Music volume, from 0 to MIX_MAX_VOLUME.
 * @param ui UI volume, from 0 to MIX_MAX_VOLUME.
 */
void Game::setVolume(int sound, int music, int ui)
{
	if (!Options::mute)
	{
		if (sound >= 0)
		{
			sound = volumeExponent(sound) * (double)SDL_MIX_MAXVOLUME;
			Mix_Volume(-1, sound);
			if (_save && _save->getSavedBattle())
			{
				Mix_Volume(3, sound * _save->getSavedBattle()->getAmbientVolume());
			}
			else
			{
				// channel 3: reserved for ambient sound effect.
				Mix_Volume(3, sound / 2);
			}
			// channel 4: reserved for unit responses
			Mix_Volume(4, sound);
		}
		if (music >= 0)
		{
			music = volumeExponent(music) * (double)SDL_MIX_MAXVOLUME;
			Mix_VolumeMusic(music);
		}
		if (ui >= 0)
		{
			ui = volumeExponent(ui) * (double)SDL_MIX_MAXVOLUME;
			Mix_Volume(1, ui);
			Mix_Volume(2, ui);
		}
	}
}

double Game::volumeExponent(int volume)
{
	return (exp(log(Game::VOLUME_GRADIENT + 1.0) * volume / (double)SDL_MIX_MAXVOLUME) -1.0 ) / Game::VOLUME_GRADIENT;
}

/**
 * Pops all the states currently in stack and pushes in the new state.
 * A shortcut for cleaning up all the old states when they're not necessary
 * like in one-way transitions.
 * @param state Pointer to the new state.
 */
void Game::setState(State *state)
{
	while (!_states.empty())
	{
		popState();
	}
	pushState(state);
	_init = false;
}

/**
 * Pushes a new state into the top of the stack and initializes it.
 * The new state will be used once the next game cycle starts.
 * @param state Pointer to the new state.
 */
void Game::pushState(State *state)
{
	resetButtonNavigationInput();
	_states.push_back(state);
	_init = false;
}

/**
 * Pops the last state from the top of the stack. Since states
 * can't actually be deleted mid-cycle, it's moved into a separate queue
 * which is cleared at the start of every cycle, so the transition
 * is seamless.
 */
void Game::popState()
{
	resetButtonNavigationInput();
	_deleted.push_back(_states.back());
	_states.pop_back();
	_init = false;
}

/**
 * Sets a new saved game for the game to use.
 * @param save Pointer to the saved game.
 */
void Game::setSavedGame(SavedGame *save)
{
	delete _save;
	_save = save;
}

/**
 * Loads the mods specified in the game options.
 */
void Game::loadMods()
{
	Mod::resetGlobalStatics();
	delete _mod;
	_mod = new Mod();
	_mod->loadAll();
}

/**
 * Sets whether the mouse is activated.
 * If it is, mouse events are processed, otherwise
 * they are ignored and the cursor is hidden.
 * @param active Is mouse activated?
 */
void Game::setMouseActive(bool active)
{
	_mouseActive = active;
	_cursor->setVisible(active);
}

/**
 * Include controller holds when controls poll SDL's physical button state,
 * for example during drag scrolling.
 */
Uint8 Game::getMouseButtonState() const
{
	return SDL_GetMouseState(nullptr, nullptr) | _joystickMouseButtons;
}

/**
 * Returns whether current state is *state
 * @param state The state to test against the stack state
 * @return Is state the current state?
 */
bool Game::isState(State *state) const
{
	return !_states.empty() && _states.back() == state;
}

/**
 * Returns whether a UfopaediaStartState is in the background.
 * @return Is there a UfopaediaStartState in the background?
 */
bool Game::containsUfopaediaStartState() const
{
	for (auto* state : _states)
	{
		auto* pedia = dynamic_cast<UfopaediaStartState*>(state);
		if (pedia)
		{
			return true;
		}
	}
	return false;
}

/**
 * Returns whether a NotesState is in the background.
 * @return Is there a NotesState in the background?
 */
bool Game::containsNotesState() const
{
	for (auto* state : _states)
	{
		auto* notes = dynamic_cast<NotesState*>(state);
		if (notes)
		{
			return true;
		}
	}
	return false;
}

/**
 * Returns the GeoscapeState from the background (if available).
 * @return Pointer to GeoscapeState, or nullptr if not available.
 */
GeoscapeState* Game::getGeoscapeState() const
{
	for (auto* state : _states)
	{
		auto* geoscape = dynamic_cast<GeoscapeState*>(state);
		if (geoscape)
		{
			return geoscape;
		}
	}
	return nullptr;
}

/**
 * Checks if the game is currently quitting.
 * @return whether the game is shutting down or not.
 */
bool Game::isQuitting() const
{
	return _quit;
}

/**
 * Loads the most appropriate languages
 * given current system and game options.
 */
void Game::loadLanguages()
{
	const std::string defaultLang = "en-US";
	std::string currentLang = defaultLang;

	// No language set, detect based on system
	if (Options::language.empty())
	{
		std::string locale = CrossPlatform::getLocale();
		std::string lang = locale.substr(0, locale.find_first_of('-'));
		// Try to load full locale
		if (Language::isSupported(locale) && FileMap::fileExists("Language/" + locale + ".yml"))
		{
			currentLang = locale;
		}
		else
		{
			// Try to load language locale
			if (Language::isSupported(lang) && FileMap::fileExists("Language/" + lang + ".yml"))
			{
				currentLang = lang;
			}
			// Give up, use default
			else
			{
				currentLang = defaultLang;
			}
		}
	}
	else
	{
		// Use options language
		if (FileMap::fileExists("Language/" + Options::language + ".yml"))
		{
			currentLang = Options::language;
		}
		// Language not found, use default
		else
		{
			currentLang = defaultLang;
		}
	}
	Options::language = currentLang;

	delete _lang;
	_lang = new Language();

	const std::string dirLanguage = "Language/";
	const std::string dirLanguageAndroid = "Language/Android/";
	const std::string dirLanguageOXCE = "Language/OXCE/";
	const std::string dirLanguageTechnical = "Language/Technical/";

	const std::string defaultLangYml = defaultLang + ".yml";
	const std::string currentLangYml = currentLang + ".yml";

	// get vertical VFS map slices for the four filenames,
	// then submit frecs in lockstep to the _lang->loadFile().

	auto slice = FileMap::getSlice(dirLanguage + defaultLangYml);
	auto sliceAndroid = FileMap::getSlice(dirLanguageAndroid + defaultLangYml);
	auto sliceOXCE = FileMap::getSlice(dirLanguageOXCE + defaultLangYml);
	auto sliceTechnical = FileMap::getSlice(dirLanguageTechnical + defaultLangYml);

	auto slice2 = FileMap::getSlice(dirLanguage + currentLangYml);
	auto sliceAndroid2 = FileMap::getSlice(dirLanguageAndroid + currentLangYml);
	auto sliceOXCE2 = FileMap::getSlice(dirLanguageOXCE + currentLangYml);
	auto sliceTechnical2 = FileMap::getSlice(dirLanguageTechnical + currentLangYml);

	bool twoLangs = currentLang != defaultLang;
	for (size_t i = 0; i < slice.size(); ++i) {
		if (slice[i]) { _lang->loadFile(slice[i]); }
		if (twoLangs && slice2[i]) { _lang->loadFile(slice2[i]); }
		if (sliceAndroid[i]) { _lang->loadFile(sliceAndroid[i]); }
		if (twoLangs && sliceAndroid2[i]) { _lang->loadFile(sliceAndroid2[i]); }
		if (sliceOXCE[i]) { _lang->loadFile(sliceOXCE[i]); }
		if (twoLangs && sliceOXCE2[i]) { _lang->loadFile(sliceOXCE2[i]); }
		if (sliceTechnical[i]) { _lang->loadFile(sliceTechnical[i]); }
		if (twoLangs && sliceTechnical2[i]) { _lang->loadFile(sliceTechnical2[i]); }
	}

	_lang->loadRule(_mod->getExtraStrings(), defaultLang);
	if (twoLangs)
		_lang->loadRule(_mod->getExtraStrings(), currentLang);
}

/**
 * Initializes the audio subsystem.
 */
void Game::initAudio()
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		Log(LOG_ERROR) << SDL_GetError();
		Log(LOG_WARNING) << "No sound device detected, audio disabled.";
		Options::mute = true;
		return;
	}

	Uint16 format = MIX_DEFAULT_FORMAT;
	if (Options::audioBitDepth == 8)
		format = AUDIO_S8;

	if (Options::audioSampleRate % 11025 != 0)
	{
		Log(LOG_WARNING) << "Custom sample rate " << Options::audioSampleRate << "Hz, audio that doesn't match will be distorted!";
		Log(LOG_WARNING) << "SDL_mixer only supports multiples of 11025Hz.";
	}
	int minChunk = Options::audioSampleRate / 11025 * 512;
	Options::audioChunkSize = std::max(minChunk, Options::audioChunkSize);

	if (Mix_OpenAudio(Options::audioSampleRate, format, MIX_DEFAULT_CHANNELS, Options::audioChunkSize) != 0)
	{
		Log(LOG_ERROR) << Mix_GetError();
		Log(LOG_WARNING) << "Sound device failed, audio disabled.";
		Options::mute = true;
	}
	else
	{
		Mix_AllocateChannels(16);
		// Set up reserved channels:
		// 0 = not used?
		// 1-2 = UI
		// 3 = ambient
		// 4 = unit responses (OXCE only)
		Mix_ReserveChannels(5);
		Mix_GroupChannels(1, 2, 0);
		Log(LOG_INFO) << "SDL_mixer initialized successfully.";
		setVolume(Options::soundVolume, Options::musicVolume, Options::uiVolume);
	}
}

/**
 * Is CTRL pressed?
 */
bool Game::isCtrlPressed(bool considerTouchButtons) const
{
	if (considerTouchButtons && _ctrl)
	{
		return true;
	}
	return (SDL_GetModState() & KMOD_CTRL) != 0;
}

/**
 * Is ALT pressed?
 */
bool Game::isAltPressed(bool considerTouchButtons) const
{
	if (considerTouchButtons && _alt)
	{
		return true;
	}
	return (SDL_GetModState() & KMOD_ALT) != 0;
}

/**
 * Is SHIFT pressed?
 */
bool Game::isShiftPressed(bool considerTouchButtons) const
{
	if (considerTouchButtons && _shift)
	{
		return true;
	}
	return (SDL_GetModState() & KMOD_SHIFT) != 0;
}

/**
 * Is LMB pressed?
 */
bool Game::isLeftClick(Action* action, bool considerTouchButtons) const
{
	if (considerTouchButtons)
	{
		return (action->getDetails()->button.button == SDL_BUTTON_LEFT) && !_rmb && !_mmb;
	}
	return (action->getDetails()->button.button == SDL_BUTTON_LEFT);
}

/**
 * Is RMB pressed?
 */
bool Game::isRightClick(Action* action, bool considerTouchButtons) const
{
	if (considerTouchButtons)
	{
		return (action->getDetails()->button.button == SDL_BUTTON_RIGHT) || ((action->getDetails()->button.button == SDL_BUTTON_LEFT) && _rmb);
	}
	return (action->getDetails()->button.button == SDL_BUTTON_RIGHT);
}

/**
 * Is MMB pressed?
 */
bool Game::isMiddleClick(Action* action, bool considerTouchButtons) const
{
	if (considerTouchButtons)
	{
		return (action->getDetails()->button.button == SDL_BUTTON_MIDDLE) || ((action->getDetails()->button.button == SDL_BUTTON_LEFT) && _mmb);
	}
	return (action->getDetails()->button.button == SDL_BUTTON_MIDDLE);
}

/**
 * Resets the touch button flags.
 */
void Game::resetTouchButtonFlags()
{
	_ctrl = false;
	_alt = false;
	_shift = false;
	_rmb = false;
	_mmb = false;
	_scrollStep = 1;
}

}
