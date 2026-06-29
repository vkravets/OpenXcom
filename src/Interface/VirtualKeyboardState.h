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
#include <memory>

namespace OpenXcom
{

class TextEdit;
class Text;
class TextButton;
class Window;
struct ParsedKeyboardLayout;

/**
 * Configurable keyboard overlay for the focused text field.
 * Y opens/closes, the D-pad navigates, A selects, X deletes, and B closes.
 */
class VirtualKeyboardState : public State
{
private:
	struct Key
	{
		TextButton *button;
		Uint16 normal, shifted;
		SDLKey special;
	};

	TextEdit *_owner;
	std::string _layoutLabel;
	std::vector<std::unique_ptr<ParsedKeyboardLayout>> _layouts;
	size_t _layoutIndex;
	bool _shifted;
	int _row, _col;
	Window *_window;
	Text *_preview, *_language;
	TextButton *_languageButton;
	Surface *_selection;
	std::vector<std::vector<Key>> _keys;
	std::vector<TextButton*> _keyButtons;
	Uint8 _dpadRepeatHat;
	Uint32 _dpadRepeatSince;
	bool _dpadRepeating;

	/// Reuses existing surfaces so a click can safely change the layout.
	void showLayout();
	TextButton *getButton(int row, int col) const;
	/// Draws a transparent selection frame without recoloring the key.
	void updateHighlight();
	/// Moves between staggered rows using the nearest key center.
	void navigate(int dRow, int dCol);
	void updateLabels();
	void activateCurrentKey();
	void sendChar(Uint16 unicode);
	void sendSpecial(SDLKey sym);

public:
	enum { TOGGLE_EVENT = 0 };
	VirtualKeyboardState(TextEdit *owner, SDL_Color *palette, const State *parent = nullptr);
	~VirtualKeyboardState() override;
	void handle(Action *action) override;
	/// Starts or cancels repeat after a physical D-pad direction change.
	void armDpadRepeat(Uint8 hat);
	/// Repeats navigation while the same physical direction remains held.
	void repeatDpad(Uint8 liveHat);
	void btnKeyClick(Action *action);
	void btnShiftClick(Action *action);
	void btnLanguageClick(Action *action);
	void btnEscClick(Action *action);
};

}
