#pragma once
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
#include "../Engine/State.h"

namespace OpenXcom
{

class TextEdit;
class TextButton;
class Window;

/**
 * Virtual on-screen keyboard overlay.
 * Shown when a TextEdit gains focus while Options::keyboardMode == KEYBOARD_VIRTUAL
 * (typically when a joystick/controller is in use). The player navigates keys with
 * the D-pad and presses a button to confirm each character. Characters are injected
 * directly into the owning TextEdit.
 */
class VirtualKeyboardState : public State
{
private:
	static const int NUM_ALPHA = 26; ///< Number of letter keys (a-z / A-Z)
	static const int NUM_NUM   = 16; ///< Number of number/symbol/action keys
	static const int ROW_SIZES[3];   ///< Keys per navigation row

	TextEdit *_owner;   ///< The TextEdit that triggered this keyboard
	bool _shifted;      ///< Whether uppercase/shifted mode is active
	int  _row;          ///< Currently highlighted row (0-2)
	int  _col;          ///< Currently highlighted column within the row

	Window      *_window;
	TextButton  *_btnAlpha[NUM_ALPHA]; ///< Letter key buttons
	TextButton  *_btnNum[NUM_NUM];     ///< Number/symbol/action buttons
	TextButton  *_btnShift;            ///< Shift-toggle button
	TextButton  *_btnEsc;              ///< Cancel/ESC button

	/// Returns the TextButton at the given navigation position.
	TextButton *getButton(int row, int col) const;
	/// Updates the visual highlight when navigating.
	void updateHighlight(int oldRow, int oldCol, int newRow, int newCol);
	/// Activates (presses) the currently highlighted key.
	void activateCurrentKey();
	/// Sends a character to the owning TextEdit.
	void sendChar(Uint16 unicode);
	/// Sends a special key (Backspace, Return, Escape) to the owning TextEdit.
	void sendSpecial(SDLKey sym);

public:
	/// Creates a new virtual keyboard linked to the given TextEdit.
	VirtualKeyboardState(TextEdit *owner, SDL_Color *palette);
	/// Cleans up the virtual keyboard.
	~VirtualKeyboardState() = default;
	/// Handles events (navigation and key selection).
	void handle(Action *action) override;
	/// Handler for clicking a letter key.
	void btnAlphaClick(Action *action);
	/// Handler for clicking a number/symbol/action key.
	void btnNumClick(Action *action);
	/// Handler for clicking the Shift button.
	void btnShiftClick(Action *action);
	/// Handler for clicking the ESC/Cancel button.
	void btnEscClick(Action *action);
};

} // namespace OpenXcom
