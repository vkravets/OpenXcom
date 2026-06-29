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
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <sstream>
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/Logger.h"
#include "../Engine/Exception.h"
#include "TextEdit.h"
#include "TextButton.h"
#include "Text.h"
#include "Window.h"

namespace OpenXcom
{

// Logical game pixels, scaled together with the rest of the interface.
static const int KEY_SIZE = 17;
static const int KEY_GAP = 3;
static const int KEY_STEP = KEY_SIZE + KEY_GAP;
static const int WIDE_KEY = 2 * KEY_SIZE + KEY_GAP;
static const int MAX_ROW_WIDTH = 14 * KEY_STEP - KEY_GAP;
static const int MAX_ROWS = 8;
static const int WINDOW_PADDING = (3 * KEY_SIZE + 2) / 4;
static const int WINDOW_SIDE_PADDING = WINDOW_PADDING;
static const int HEADER_TEXT_HEIGHT = 12;
static const int HEADER_TEXT_OFFSET = (KEY_SIZE - HEADER_TEXT_HEIGHT + 1) / 2;
static const int LANGUAGE_TEXT_WIDTH = 43;
static const Uint32 DPAD_REPEAT_DELAY = 400;
static const Uint32 DPAD_REPEAT_INTERVAL = 100;

static int getWindowPadding(size_t rows)
{
	// Keep the maximum eight-row layout inside the logical viewport.
	return std::min(WINDOW_PADDING, (200 - KEY_SIZE - (int)rows * KEY_STEP) / 2);
}

struct ParsedKeyboardKey
{
	Uint16 normal = 0, shifted = 0;
	SDLKey special = SDLK_UNKNOWN;
	int width = KEY_SIZE;
	bool stretch = false;
};

struct ParsedKeyboardLayout
{
	std::string label;
	std::vector<std::vector<ParsedKeyboardKey>> rows;
	int width = 0;
};

/** Split rows before tokens so a literal single | remains a normal key. */
static bool splitLayoutRows(const std::string &source, std::vector<std::vector<std::string>> &rows, std::string &error)
{
	size_t start = 0;
	while (true)
	{
		size_t end = source.find("||", start);
		std::istringstream stream(source.substr(start, end == std::string::npos ? end : end - start));
		std::vector<std::string> row;
		std::string token;
		while (stream >> token)
		{
			row.push_back(token);
			if (row.size() > 14)
			{
				error = "row " + std::to_string(rows.size() + 1) + " has more than 14 keys";
				return false;
			}
		}
		if (row.empty())
		{
			// A final || terminates the last row; it does not add an empty one.
			if (end == std::string::npos && !rows.empty())
				return true;
			error = "empty row " + std::to_string(rows.size() + 1);
			return false;
		}
		rows.push_back(std::move(row));
		if (rows.size() > MAX_ROWS)
		{
			error = "more than " + std::to_string(MAX_ROWS) + " rows";
			return false;
		}
		if (end == std::string::npos)
			return true;
		start = end + 2;
	}
}

static bool parseKeyToken(const std::string &token, ParsedKeyboardKey &key)
{
	if (token == "shift")
	{
		key.special = SDLK_LSHIFT;
		key.width = WIDE_KEY;
	}
	else if (token == "backspace")
	{
		key.special = SDLK_BACKSPACE;
		key.width = WIDE_KEY;
	}
	else if (token == "space")
	{
		key.normal = ' ';
		key.width = WIDE_KEY;
		key.stretch = true;
	}
	else
	{
		if (!Unicode::isValidUTF8(token))
			return false;
		const UString characters = Unicode::convUtf8ToUtf32(token);
		if (characters.size() != 1)
			return false;
		UCode c = characters[0];
		// SDL 1.2 text input carries one UTF-16 code unit. Reject unsupported
		// characters instead of silently truncating them or injecting controls.
		if (c < 0x21 || (c >= 0x7f && c <= 0x9f) || c > 0xffff || (c >= 0xd800 && c <= 0xdfff))
			return false;
		key.normal = (Uint16)c;
	}
	return true;
}

/** Validate the entire layout before creating any interactive surfaces. */
static bool parseLayout(const Options::VirtualKeyboardLayoutOptions &config, ParsedKeyboardLayout &layout, std::string &error)
{
	std::vector<std::vector<std::string>> normal, shifted;
	if (!splitLayoutRows(config.normal, normal, error))
		return false;
	const std::string &shiftedSource = config.shifted.find_first_not_of(" \t\r\n\f\v") == std::string::npos ? config.normal : config.shifted;
	if (!splitLayoutRows(shiftedSource, shifted, error))
	{
		error = "shifted: " + error;
		return false;
	}
	if (normal.size() != shifted.size())
	{
		error = "normal and shifted must have the same rows";
		return false;
	}

	ParsedKeyboardLayout parsed;
	parsed.label = config.label;
	for (size_t row = 0; row < normal.size(); ++row)
	{
		if (normal[row].size() != shifted[row].size())
		{
			error = "normal and shifted key counts differ in row " + std::to_string(row + 1);
			return false;
		}
		std::vector<ParsedKeyboardKey> keys;
		int width = KEY_GAP * ((int)normal[row].size() - 1);
		int spaces = 0;
		for (size_t col = 0; col < normal[row].size(); ++col)
		{
			ParsedKeyboardKey key, shiftedKey;
			if (!parseKeyToken(normal[row][col], key) || !parseKeyToken(shifted[row][col], shiftedKey))
			{
				error = "invalid key in row " + std::to_string(row + 1) + ", column " + std::to_string(col + 1);
				return false;
			}
			if (key.special != shiftedKey.special || key.stretch != shiftedKey.stretch)
			{
				error = "special keys must match in normal and shifted, row " + std::to_string(row + 1);
				return false;
			}
			key.shifted = shiftedKey.normal;
			width += key.width;
			spaces += key.stretch ? 1 : 0;
			keys.push_back(key);
		}
		if (row == 0)
		{
			// Space has its minimum width in this calculation, avoiding a
			// circular dependency when the first row itself contains a space.
			if (width > MAX_ROW_WIDTH)
			{
				error = "first row exceeds " + std::to_string(MAX_ROW_WIDTH) + " pixels";
				return false;
			}
			parsed.width = width;
		}
		else if (width > parsed.width)
		{
			error = "row " + std::to_string(row + 1) + " is wider than the first row";
			return false;
		}
		if (spaces != 0)
		{
			int extra = (parsed.width - width) / spaces;
			int remainder = (parsed.width - width) % spaces;
			for (auto &key : keys)
			{
				if (key.stretch)
				{
					key.width += extra;
					if (remainder > 0)
					{
						++key.width;
						--remainder;
					}
				}
			}
		}
		parsed.rows.push_back(std::move(keys));
	}
	layout = std::move(parsed);
	error.clear();
	return true;
}

static std::vector<std::unique_ptr<ParsedKeyboardLayout>> selectLayouts()
{
	std::vector<std::unique_ptr<ParsedKeyboardLayout>> layouts;
	std::vector<std::string> added;
	for (const std::string &id : Options::oxceVirtualKeyboardLanguages)
	{
		if (std::find(added.begin(), added.end(), id) != added.end())
			continue;
		auto entry = Options::oxceVirtualKeyboardLayouts.find(id);
		if (entry == Options::oxceVirtualKeyboardLayouts.end())
			continue;
		std::unique_ptr<ParsedKeyboardLayout> layout(new ParsedKeyboardLayout);
		std::string error;
		if (parseLayout(entry->second, *layout, error))
		{
			if (layout->label.empty())
				layout->label = id;
			layouts.push_back(std::move(layout));
			added.push_back(id);
		}
		else
			Log(LOG_WARNING) << "Ignoring virtual keyboard layout '" << id << "': " << error;
	}
	// This definition is independent of the editable map, so a malformed
	// override of en-US cannot break text entry or overwrite the user's data.
	if (layouts.empty())
	{
		std::unique_ptr<ParsedKeyboardLayout> fallback(new ParsedKeyboardLayout);
		std::string error;
		if (!parseLayout(Options::getDefaultVirtualKeyboardLayout(), *fallback, error))
			throw Exception("Invalid built-in virtual keyboard layout: " + error);
		layouts.push_back(std::move(fallback));
	}
	return layouts;
}

VirtualKeyboardState::VirtualKeyboardState(TextEdit *owner, SDL_Color *palette, const State *parent)
	: _owner(owner), _layouts(selectLayouts()), _layoutIndex(0), _shifted(false), _row(0), _col(0),
	  _language(nullptr), _languageButton(nullptr), _dpadRepeatHat(SDL_HAT_CENTERED),
	  _dpadRepeatSince(0), _dpadRepeating(false)
{
	_row = _layouts[0]->rows.size() > 1 ? 1 : 0;
	_screen = false;
	setStatePalette(palette);

	const int windowPadding = getWindowPadding(_layouts[0]->rows.size());
	const int windowHeight = 2 * windowPadding + KEY_SIZE + (int)_layouts[0]->rows.size() * KEY_STEP;
	const int windowY = 200 - windowHeight;
	const int windowWidth = _layouts[0]->width + 2 * WINDOW_SIDE_PADDING;
	_window = new Window(this, windowWidth, windowHeight, (320 - windowWidth + 1) / 2, windowY);
	_window->setColor(_owner->getColor());
	if (parent)
		parent->applyOverlayStyle(_window, nullptr);
	add(_window);

	// Mirror validated text when the overlay covers the original field.
	_preview = new Text(1, HEADER_TEXT_HEIGHT, 0, windowY + windowPadding + HEADER_TEXT_OFFSET);
	add(_preview);
	_preview->setColor(_owner->getColor());
	_preview->setText(_owner->getText());
	if (_layouts.size() > 1)
	{
		_languageButton = new TextButton(KEY_SIZE, KEY_SIZE, 0, windowY + windowPadding);
		_languageButton->setColor(_owner->getColor());
		if (parent)
			parent->applyOverlayStyle(nullptr, _languageButton);
		add(_languageButton);
		_languageButton->setTextPadding(3, 3);
		_languageButton->onMouseClick((ActionHandler)&VirtualKeyboardState::btnLanguageClick);
	}
	else
	{
		_language = new Text(LANGUAGE_TEXT_WIDTH, HEADER_TEXT_HEIGHT, 0, windowY + windowPadding + HEADER_TEXT_OFFSET);
		add(_language);
		_language->setColor(_owner->getColor());
		_language->setAlign(ALIGN_RIGHT);
	}

	// Allocate once: changing layouts from a click must not invalidate
	// State::handle's iterators or delete a button still processing an event.
	size_t maxKeys = 0;
	for (const auto &layout : _layouts)
	{
		size_t count = 0;
		for (const auto &row : layout->rows)
			count += row.size();
		maxKeys = std::max(maxKeys, count);
	}
	for (size_t i = 0; i < maxKeys; ++i)
	{
		TextButton *button = new TextButton(KEY_SIZE, KEY_SIZE);
		button->setColor(_owner->getColor());
		if (parent)
			parent->applyOverlayStyle(nullptr, button);
		add(button);
		button->setTextPadding(3, 3);
		button->onMouseClick((ActionHandler)&VirtualKeyboardState::btnKeyClick);
		_keyButtons.push_back(button);
	}

	// A transparent frame leaves the button's own fill and click behavior intact.
	_selection = new Surface(KEY_SIZE + 2, KEY_SIZE + 2);
	add(_selection);
	showLayout();
	centerAllSurfaces();
	updateHighlight();
}

VirtualKeyboardState::~VirtualKeyboardState() = default;

void VirtualKeyboardState::showLayout()
{
	const ParsedKeyboardLayout &layout = *_layouts[_layoutIndex];
	_layoutLabel = layout.label;
	// Derive offsets from the existing window to preserve centering after
	// display resizes as well as when switching between different row counts.
	const int offsetX = _window->getX() - (320 - _window->getWidth() + 1) / 2;
	const int bottom = _window->getY() + _window->getHeight();
	const int windowWidth = layout.width + 2 * WINDOW_SIDE_PADDING;
	const int windowX = (320 - windowWidth + 1) / 2;
	const int windowPadding = getWindowPadding(layout.rows.size());
	const int height = 2 * windowPadding + KEY_SIZE + (int)layout.rows.size() * KEY_STEP;
	const int y = bottom - height;
	_window->setWidth(windowWidth);
	_window->setHeight(height);
	_window->setX(windowX + offsetX);
	_window->setY(y);
	_window->setDX(-windowX);
	_window->setDY(height - 200);
	_preview->setY(y + windowPadding + HEADER_TEXT_OFFSET);
	if (_languageButton)
		_languageButton->setY(y + windowPadding);
	else
	{
		_language->setY(y + windowPadding + HEADER_TEXT_OFFSET);
		_language->setWidth(std::min(LANGUAGE_TEXT_WIDTH, layout.width));
	}
	for (auto *button : _keyButtons)
		button->setVisible(false);
	_keys.clear();
	_keys.resize(layout.rows.size());
	size_t buttonIndex = 0;
	for (int row = 0; row < (int)layout.rows.size(); ++row)
	{
		const auto &keys = layout.rows[row];
		int width = KEY_GAP * ((int)keys.size() - 1);
		for (const auto &key : keys)
			width += key.width;
		int x = offsetX + (320 - width + 1) / 2;
		for (const auto &key : keys)
		{
			TextButton *button = _keyButtons[buttonIndex++];
			button->setX(x);
			button->setY(y + windowPadding + KEY_STEP + row * KEY_STEP);
			button->setWidth(key.width);
			button->setVisible(true);
			_keys[row].push_back({ button, key.normal, key.shifted, key.special });
			x += key.width + KEY_GAP;
		}
	}
	// Follow the current grid, including its centering offset and any custom
	// first-row width. Screen scales these logical coordinates for rendering.
	const TextButton *lastKey = _keys.front().back().button;
	Surface *language = _languageButton ? static_cast<Surface*>(_languageButton) : _language;
	language->setX(lastKey->getX() + lastKey->getWidth() - language->getWidth());
	_preview->setX(_keys.front().front().button->getX());
	const int previewWidth = language->getX() - KEY_GAP - _preview->getX();
	// A one-key custom layout may leave no header room beside the language.
	_preview->setWidth(std::max(1, previewWidth));
	_preview->setVisible(previewWidth > 0);
	updateLabels();
	updateHighlight();
}

TextButton *VirtualKeyboardState::getButton(int row, int col) const
{
	if (row == -1 && col == 0)
		return _languageButton;
	if (row < 0 || row >= (int)_keys.size() || col < 0 || col >= (int)_keys[row].size())
		return nullptr;
	return _keys[row][col].button;
}

void VirtualKeyboardState::updateHighlight()
{
	TextButton *button = getButton(_row, _col);
	if (!button)
		return;

	// Pick a bright shade from the button's own palette ramp, preserving the
	// dialog's hue in both UFO/TFTD and modded palettes. Index 0 is transparent.
	int start = (button->getColor() / 16) * 16;
	Uint8 color = button->getColor();
	int brightest = -1;
	for (int i = start; i < start + 16; ++i)
	{
		if (i == 0)
			continue;
		const SDL_Color &shade = _palette[i];
		int brightness = 299 * shade.r + 587 * shade.g + 114 * shade.b;
		if (brightness > brightest)
		{
			brightest = brightness;
			color = (Uint8)i;
		}
	}
	_selection->setX(button->getX() - 1);
	_selection->setY(button->getY() - 1);
	_selection->setWidth(button->getWidth() + 2);
	_selection->setHeight(button->getHeight() + 2);
	_selection->draw();
	const int width = _selection->getWidth(), height = _selection->getHeight();
	_selection->drawRect(0, 0, width, 1, color);
	_selection->drawRect(0, height - 1, width, 1, color);
	_selection->drawRect(0, 1, 1, height - 2, color);
	_selection->drawRect(width - 1, 1, 1, height - 2, color);
}

void VirtualKeyboardState::navigate(int dRow, int dCol)
{
	if (dRow != 0)
	{
		TextButton *current = getButton(_row, _col);
		int center = current->getX() * 2 + current->getWidth();
		const int firstRow = _languageButton ? -1 : 0;
		const int count = (int)_keys.size() - firstRow;
		_row = firstRow + (_row - firstRow + dRow + count) % count;
		_col = 0;
		if (_row == -1)
		{
			updateHighlight();
			return;
		}
		int closest = INT_MAX;
		for (int col = 0; col < (int)_keys[_row].size(); ++col)
		{
			TextButton *button = getButton(_row, col);
			int distance = std::abs(button->getX() * 2 + button->getWidth() - center);
			if (distance < closest)
			{
				closest = distance;
				_col = col;
			}
		}
	}
	else if (dCol != 0 && _row >= 0)
	{
		int count = (int)_keys[_row].size();
		_col = (_col + dCol + count) % count;
	}
	updateHighlight();
}

void VirtualKeyboardState::armDpadRepeat(Uint8 hat)
{
	const Uint8 requiredFocus = SDL_APPINPUTFOCUS | SDL_APPACTIVE;
	if (!_game->isState(this) || !Options::oxceJoystickEnabled ||
		(SDL_GetAppState() & requiredFocus) != requiredFocus)
		hat = SDL_HAT_CENTERED;
	_dpadRepeatHat = hat;
	_dpadRepeatSince = SDL_GetTicks();
	_dpadRepeating = false;
}

void VirtualKeyboardState::repeatDpad(Uint8 liveHat)
{
	const Uint8 requiredFocus = SDL_APPINPUTFOCUS | SDL_APPACTIVE;
	if (!_game->isState(this) || !Options::oxceJoystickEnabled ||
		(SDL_GetAppState() & requiredFocus) != requiredFocus || liveHat != _dpadRepeatHat)
	{
		armDpadRepeat(SDL_HAT_CENTERED);
		return;
	}
	if (_dpadRepeatHat == SDL_HAT_CENTERED)
		return;
	const Uint32 now = SDL_GetTicks();
	const Uint32 interval = _dpadRepeating ? DPAD_REPEAT_INTERVAL : DPAD_REPEAT_DELAY;
	if ((Uint32)(now - _dpadRepeatSince) < interval)
		return;
	// Never queue repeat events or catch up missed ticks after a slow frame:
	// a repeat belongs only to this currently active keyboard.
	_dpadRepeatSince = now;
	_dpadRepeating = true;
	if (_dpadRepeatHat & SDL_HAT_UP) navigate(-1, 0);
	if (_dpadRepeatHat & SDL_HAT_DOWN) navigate(1, 0);
	if (_dpadRepeatHat & SDL_HAT_LEFT) navigate(0, -1);
	if (_dpadRepeatHat & SDL_HAT_RIGHT) navigate(0, 1);
}

void VirtualKeyboardState::updateLabels()
{
	if (_languageButton)
		_languageButton->setText(Unicode::convUtf32ToUtf8(Unicode::convUtf8ToUtf32(_layoutLabel).substr(0, 2)));
	else
		_language->setText(_layoutLabel + (_shifted ? " ^" : ""));
	for (auto &row : _keys)
	{
		for (auto &key : row)
		{
			switch (key.special)
			{
			case SDLK_BACKSPACE: key.button->setText("<-"); break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT: key.button->setText(_shifted ? "SHIFT" : "Shift"); break;
			default:
				key.button->setText(key.normal == ' ' ? "Space" :
					Unicode::convUtf32ToUtf8(UString(1, _shifted ? key.shifted : key.normal)));
				break;
			}
		}
	}
}

void VirtualKeyboardState::activateCurrentKey()
{
	if (_row == -1)
	{
		btnLanguageClick(nullptr);
		return;
	}
	const Key &key = _keys[_row][_col];
	if (key.special == SDLK_LSHIFT || key.special == SDLK_RSHIFT)
		btnShiftClick(nullptr);
	else if (key.special != SDLK_UNKNOWN)
		sendSpecial(key.special);
	else
		sendChar(_shifted ? key.shifted : key.normal);
}

void VirtualKeyboardState::sendChar(Uint16 unicode)
{
	_owner->typeVirtualKey(SDLK_UNKNOWN, unicode);
	if (_game->isState(this))
		_preview->setText(_owner->getText());
}

/** Dismiss before confirmation callbacks can change the parent's state stack. */
void VirtualKeyboardState::sendSpecial(SDLKey sym)
{
	if (sym == SDLK_ESCAPE)
	{
		btnEscClick(nullptr);
		return;
	}
	if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER)
	{
		_owner->setFocus(false);
		_game->popState();
	}
	_owner->typeVirtualKey(sym, 0);
	if (_game->isState(this))
		_preview->setText(_owner->getText());
}

void VirtualKeyboardState::handle(Action *action)
{
	if (action->getDetails()->type == SDL_USEREVENT &&
		action->getDetails()->user.code == TOGGLE_EVENT)
	{
		btnEscClick(nullptr);
		return;
	}

	State::handle(action);
	if (!_game->isState(this))
		return;

	if (action->getDetails()->type == SDL_MOUSEMOTION)
	{
		int x = action->getAbsoluteXMouse(), y = action->getAbsoluteYMouse();
		if (_languageButton && x >= _languageButton->getX() &&
			x < _languageButton->getX() + _languageButton->getWidth() &&
			y >= _languageButton->getY() && y < _languageButton->getY() + _languageButton->getHeight())
		{
			_row = -1;
			_col = 0;
			updateHighlight();
			return;
		}
		for (int row = 0; row < (int)_keys.size(); ++row)
		{
			for (int col = 0; col < (int)_keys[row].size(); ++col)
			{
				TextButton *button = getButton(row, col);
				if (x >= button->getX() && x < button->getX() + button->getWidth() &&
					y >= button->getY() && y < button->getY() + button->getHeight())
				{
					if (row != _row || col != _col)
					{
						_row = row;
						_col = col;
						updateHighlight();
					}
					return;
				}
			}
		}
		return;
	}

	if (action->getDetails()->type != SDL_KEYDOWN)
		return;
	SDLKey sym = action->getDetails()->key.keysym.sym;
	switch (sym)
	{
	case SDLK_LEFT: navigate(0, -1); break;
	case SDLK_RIGHT: navigate(0, 1); break;
	case SDLK_UP: navigate(-1, 0); break;
	case SDLK_DOWN: navigate(1, 0); break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
	case SDLK_SPACE: activateCurrentKey(); break;
	case SDLK_BACKSPACE:
	case SDLK_ESCAPE: sendSpecial(sym); break;
	default: break;
	}
}

void VirtualKeyboardState::btnKeyClick(Action *action)
{
	for (int row = 0; row < (int)_keys.size(); ++row)
	{
		for (int col = 0; col < (int)_keys[row].size(); ++col)
		{
			if (getButton(row, col) == action->getSender())
			{
				_row = row;
				_col = col;
				updateHighlight();
				activateCurrentKey();
				return;
			}
		}
	}
}

void VirtualKeyboardState::btnShiftClick(Action * /*action*/)
{
	_shifted = !_shifted;
	updateLabels();
}

void VirtualKeyboardState::btnLanguageClick(Action * /*action*/)
{
	if (_layouts.size() < 2)
		return;
	_layoutIndex = (_layoutIndex + 1) % _layouts.size();
	_row = -1;
	_col = 0;
	showLayout();
}

/** Close without confirming; leave focus intact so Y can reopen the keyboard. */
void VirtualKeyboardState::btnEscClick(Action * /*action*/)
{
	_game->popState();
}

}
