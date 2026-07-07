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
#include "VirtualKeyboardState.h"
#include <SDL.h>
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "TextEdit.h"
#include "TextButton.h"
#include "Window.h"

namespace OpenXcom
{

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

// Row 0: 26 letter keys (a-z / A-Z)
// Row 1: 16 number/symbol/action keys (0-9, -, _, ., SPC, BSP, OK)
// Row 2: 2 special-function keys (Shift, ESC)
const int VirtualKeyboardState::ROW_SIZES[3] = { 26, 16, 2 };

// Keyboard window geometry (in game-coordinate pixels, before centering)
static const int WIN_X = 0;
static const int WIN_Y = 120;
static const int WIN_W = 320;
static const int WIN_H = 80;

// Button size and gap
static const int BTN_W = 11;
static const int BTN_H = 11;
static const int BTN_GAP = 1;
static const int BTN_STEP = BTN_W + BTN_GAP; // 12 px per slot

// Row Y positions (relative to top of screen, before centerAllSurfaces)
static const int ROW0_Y = 123;
static const int ROW1_Y = 136;
static const int ROW2_Y = 149;

// Row 0 starts at x=4 (centres 26*12-1 = 311px in 320px)
static const int ROW0_X = 4;
// Row 1 starts at x=64 (centres 16*12-1 = 191px in 320px)
static const int ROW1_X = 64;

// Number/symbol characters for row 1 (indices 0-12)
static const char NUM_CHARS[] = "0123456789-_.";  // 13 printable chars
// indices 13 = space, 14 = Backspace, 15 = OK

// Width of the wide special-function buttons in row 2
static const int SPECIAL_BTN_W = 32;
// Horizontal centres for SHIFT and ESC in row 2
static const int SHIFT_X = 80;
static const int ESC_X   = 208;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

/**
 * Creates the virtual on-screen keyboard.
 * @param owner   TextEdit that triggered this state.
 * @param palette Palette from the calling state.
 */
VirtualKeyboardState::VirtualKeyboardState(TextEdit *owner, SDL_Color *palette)
	: _owner(owner), _shifted(false), _row(0), _col(0)
{
	_screen = false; // overlay – do not erase previous state

	// --- Window ---
	_window = new Window(this, WIN_W, WIN_H, WIN_X, WIN_Y);

	// --- Row 0: letter keys ---
	for (int i = 0; i < NUM_ALPHA; ++i)
	{
		_btnAlpha[i] = new TextButton(BTN_W, BTN_H,
		                              ROW0_X + i * BTN_STEP, ROW0_Y);
	}

	// --- Row 1: number/symbol/action keys ---
	for (int i = 0; i < NUM_NUM; ++i)
	{
		_btnNum[i] = new TextButton(BTN_W, BTN_H,
		                            ROW1_X + i * BTN_STEP, ROW1_Y);
	}

	// --- Row 2: Shift and ESC ---
	_btnShift = new TextButton(SPECIAL_BTN_W, BTN_H, SHIFT_X, ROW2_Y);
	_btnEsc   = new TextButton(SPECIAL_BTN_W, BTN_H, ESC_X,   ROW2_Y);

	// Apply palette
	setStatePalette(palette);

	// Add all surfaces to the state
	add(_window);
	for (int i = 0; i < NUM_ALPHA; ++i)
		add(_btnAlpha[i]);
	for (int i = 0; i < NUM_NUM; ++i)
		add(_btnNum[i]);
	add(_btnShift);
	add(_btnEsc);

	// Shift every surface down/right to sit within the scaled viewport
	centerAllSurfaces();

	// --- Window colour ---
	Uint8 color = 1; // default colour index; looks reasonable on most palettes
	_window->setColor(color);
	_window->setHighContrast(false);

	// --- Row 0 labels & handlers ---
	for (int i = 0; i < NUM_ALPHA; ++i)
	{
		char label[2] = { (char)('a' + i), '\0' };
		_btnAlpha[i]->setColor(color);
		_btnAlpha[i]->setText(label);
		_btnAlpha[i]->onMouseClick((ActionHandler)&VirtualKeyboardState::btnAlphaClick);
	}

	// --- Row 1 labels & handlers ---
	for (int i = 0; i < 13; ++i)
	{
		char label[2] = { NUM_CHARS[i], '\0' };
		_btnNum[i]->setColor(color);
		_btnNum[i]->setText(label);
		_btnNum[i]->onMouseClick((ActionHandler)&VirtualKeyboardState::btnNumClick);
	}
	// Index 13 = space
	_btnNum[13]->setColor(color);
	_btnNum[13]->setText("SP");
	_btnNum[13]->onMouseClick((ActionHandler)&VirtualKeyboardState::btnNumClick);
	// Index 14 = Backspace
	_btnNum[14]->setColor(color);
	_btnNum[14]->setText("<-");
	_btnNum[14]->onMouseClick((ActionHandler)&VirtualKeyboardState::btnNumClick);
	// Index 15 = OK / Enter
	_btnNum[15]->setColor(color);
	_btnNum[15]->setText("OK");
	_btnNum[15]->onMouseClick((ActionHandler)&VirtualKeyboardState::btnNumClick);

	// --- Row 2 labels & handlers ---
	_btnShift->setColor(color);
	_btnShift->setText("SHF");
	_btnShift->onMouseClick((ActionHandler)&VirtualKeyboardState::btnShiftClick);

	_btnEsc->setColor(color);
	_btnEsc->setText("ESC");
	_btnEsc->onMouseClick((ActionHandler)&VirtualKeyboardState::btnEscClick);

	// Highlight the initial key (row 0, col 0)
	updateHighlight(-1, -1, 0, 0);
}

// ---------------------------------------------------------------------------
// Helper: button at (row, col)
// ---------------------------------------------------------------------------

/**
 * Returns the TextButton at navigation position (row, col).
 */
TextButton *VirtualKeyboardState::getButton(int row, int col) const
{
	switch (row)
	{
	case 0: return _btnAlpha[col];
	case 1: return _btnNum[col];
	case 2: return (col == 0) ? _btnShift : _btnEsc;
	default: return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Highlight management
// ---------------------------------------------------------------------------

/**
 * Updates the visual highlight, removing it from the old button and
 * setting it on the new one by swapping colour offsets.
 */
void VirtualKeyboardState::updateHighlight(int oldRow, int oldCol, int newRow, int newCol)
{
	// De-highlight old button
	if (oldRow >= 0 && oldRow < 3 && oldCol >= 0)
	{
		TextButton *btn = getButton(oldRow, oldCol);
		if (btn)
		{
			btn->setColor(1);
			btn->draw();
		}
	}
	// Highlight new button by using a brighter colour index
	if (newRow >= 0 && newRow < 3 && newCol >= 0)
	{
		TextButton *btn = getButton(newRow, newCol);
		if (btn)
		{
			btn->setColor(7); // brighter colour in most game palettes
			btn->draw();
		}
	}
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

/**
 * Moves the highlight by (dRow, dCol), with wrapping within each row
 * and clamping of the column when switching rows.
 */
static void navigate(int &row, int &col, int dRow, int dCol, const int rowSizes[3])
{
	int oldRow = row;
	row = (row + dRow + 3) % 3;
	// Wrap column within the new row
	if (dCol != 0)
		col = (col + dCol + rowSizes[oldRow]) % rowSizes[oldRow];
	// Clamp column to valid range of new row when changing rows
	if (dRow != 0 && col >= rowSizes[row])
		col = rowSizes[row] - 1;
}

// ---------------------------------------------------------------------------
// Key activation
// ---------------------------------------------------------------------------

/**
 * Activates the currently highlighted key.
 */
void VirtualKeyboardState::activateCurrentKey()
{
	switch (_row)
	{
	case 0:
	{
		// Letter key
		Uint16 unicode = _shifted
		                 ? (Uint16)('A' + _col)
		                 : (Uint16)('a' + _col);
		sendChar(unicode);
		break;
	}
	case 1:
	{
		// Number / symbol / action key
		if (_col < 13)
		{
			sendChar((Uint16)NUM_CHARS[_col]);
		}
		else if (_col == 13)
		{
			sendChar((Uint16)' ');
		}
		else if (_col == 14)
		{
			sendSpecial(SDLK_BACKSPACE);
		}
		else // _col == 15
		{
			sendSpecial(SDLK_RETURN);
		}
		break;
	}
	case 2:
	{
		if (_col == 0)
		{
			// Shift toggle
			Action dummy(nullptr, 1.0, 1.0, 0, 0);
			btnShiftClick(&dummy);
		}
		else
		{
			// ESC / Cancel
			Action dummy(nullptr, 1.0, 1.0, 0, 0);
			btnEscClick(&dummy);
		}
		break;
	}
	default:
		break;
	}
}

/**
 * Sends a printable character to the owning TextEdit.
 */
void VirtualKeyboardState::sendChar(Uint16 unicode)
{
	_owner->typeVirtualKey(SDLK_UNKNOWN, unicode);
}

/**
 * Sends a special key (Backspace, Return, Escape) to the owning TextEdit.
 * If the key causes the TextEdit to close (Return / Escape), this state
 * pops itself immediately afterwards.
 */
void VirtualKeyboardState::sendSpecial(SDLKey sym)
{
	_owner->typeVirtualKey(sym, 0);
	// Return and Escape will unfocus the TextEdit; we should also dismiss
	// this overlay so the previous state regains full control.
	if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_ESCAPE)
	{
		_game->popState();
	}
}

// ---------------------------------------------------------------------------
// State::handle override
// ---------------------------------------------------------------------------

/**
 * Handles keyboard events for D-pad navigation and key selection.
 * Mouse events are handled automatically by the TextButton click handlers.
 */
void VirtualKeyboardState::handle(Action *action)
{
	// Let surfaces process mouse events normally
	State::handle(action);

	if (action->getDetails()->type != SDL_KEYDOWN)
		return;

	SDLKey sym = action->getDetails()->key.keysym.sym;
	int oldRow = _row;
	int oldCol = _col;

	switch (sym)
	{
	case SDLK_LEFT:
		navigate(_row, _col, 0, -1, ROW_SIZES);
		updateHighlight(oldRow, oldCol, _row, _col);
		break;
	case SDLK_RIGHT:
		navigate(_row, _col, 0, +1, ROW_SIZES);
		updateHighlight(oldRow, oldCol, _row, _col);
		break;
	case SDLK_UP:
		navigate(_row, _col, -1, 0, ROW_SIZES);
		updateHighlight(oldRow, oldCol, _row, _col);
		break;
	case SDLK_DOWN:
		navigate(_row, _col, +1, 0, ROW_SIZES);
		updateHighlight(oldRow, oldCol, _row, _col);
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
	case SDLK_SPACE:
		activateCurrentKey();
		break;
	case SDLK_BACKSPACE:
		sendSpecial(SDLK_BACKSPACE);
		break;
	case SDLK_ESCAPE:
		// Cancel – pop the keyboard without confirming
		sendSpecial(SDLK_ESCAPE);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Button click handlers
// ---------------------------------------------------------------------------

/**
 * Handles a click on a letter key.
 */
void VirtualKeyboardState::btnAlphaClick(Action *action)
{
	// Find which button was clicked
	InteractiveSurface *sender = action->getSender();
	for (int i = 0; i < NUM_ALPHA; ++i)
	{
		if (_btnAlpha[i] == sender)
		{
			// Update navigation highlight to follow the mouse click
			int oldRow = _row, oldCol = _col;
			_row = 0;
			_col = i;
			updateHighlight(oldRow, oldCol, _row, _col);

			Uint16 unicode = _shifted
			                 ? (Uint16)('A' + i)
			                 : (Uint16)('a' + i);
			sendChar(unicode);
			return;
		}
	}
}

/**
 * Handles a click on a number/symbol/action key.
 */
void VirtualKeyboardState::btnNumClick(Action *action)
{
	InteractiveSurface *sender = action->getSender();
	for (int i = 0; i < NUM_NUM; ++i)
	{
		if (_btnNum[i] == sender)
		{
			int oldRow = _row, oldCol = _col;
			_row = 1;
			_col = i;
			updateHighlight(oldRow, oldCol, _row, _col);

			if (i < 13)
				sendChar((Uint16)NUM_CHARS[i]);
			else if (i == 13)
				sendChar((Uint16)' ');
			else if (i == 14)
				sendSpecial(SDLK_BACKSPACE);
			else // i == 15
				sendSpecial(SDLK_RETURN);
			return;
		}
	}
}

/**
 * Toggles the shift (uppercase) mode.
 */
void VirtualKeyboardState::btnShiftClick(Action * /*action*/)
{
	_shifted = !_shifted;
	// Update letter button labels
	for (int i = 0; i < NUM_ALPHA; ++i)
	{
		char label[2] = { (char)(_shifted ? 'A' + i : 'a' + i), '\0' };
		_btnAlpha[i]->setText(label);
		_btnAlpha[i]->draw();
	}
	// Visually distinguish the Shift button when active
	_btnShift->setColor(_shifted ? 7 : 1);
	_btnShift->draw();
	// Re-apply highlight in case it's on a letter button
	TextButton *highlighted = getButton(_row, _col);
	if (highlighted)
	{
		highlighted->setColor(7);
		highlighted->draw();
	}
}

/**
 * Closes the virtual keyboard without confirming.
 */
void VirtualKeyboardState::btnEscClick(Action * /*action*/)
{
	_game->popState();
}

} // namespace OpenXcom
