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
#include <vector>
#include <string>
#include <SDL.h>
#include "LocalizedText.h"

namespace OpenXcom
{

class Game;
class Surface;
class InteractiveSurface;
class Window;
class TextButton;
class TextEdit;
class Action;
class SavedBattleGame;
class RuleInterface;
class Sound;

enum SoldierGender : char;

/**
 * A game state that receives user input and reacts accordingly.
 * Game states typically represent a whole window or screen that
 * the user interacts with, making the game... well, interactive.
 * They automatically handle child elements used to transmit
 * information from/to the user, and are linked to the core game
 * engine which manages them.
 */
class State
{
	friend class Timer;

protected:
	static Game *_game;
	std::vector<Surface*> _surfaces;
	std::vector<Surface*> _surfacesOwned;
	bool _screen;
	bool _soundPlayed;
	InteractiveSurface *_modal;
	RuleInterface *_ruleInterface;
	RuleInterface *_ruleInterfaceParent;
	const Sound* _customSound;

	SDL_Color _palette[256];
	Uint8 _cursorColor;
private:
	InteractiveSurface *_navigationButton;
	Surface *_navigationFrame;
	bool _navigationEditing;
	std::vector<InteractiveSurface*> getNavigationButtons() const;
	void finishNavigationControl(bool cancel);
public:
	/// Creates a new state linked to a game.
	State();
	/// Cleans up the state.
	virtual ~State();
	/// Set interface rules.
	void setInterface(const std::string &s, bool alterPal = false, SavedBattleGame *battleGame = 0);
	/// Set window background.
	void setWindowBackground(Window *window, const std::string &s);
	/// Set window background by image name (instead of by interface name).
	void setWindowBackgroundImage(Window* window, const std::string& bgImageName);
	/// Applies this state's current window/button appearance to an overlay.
	void applyOverlayStyle(Window *window, TextButton *button) const;
	/// Add a optional child element but it will not be displayed.
	template<typename T>
	T* preAdd(T *surface)
	{
		static_assert(std::is_base_of_v<Surface, T>, "Type need to be surface");
		preAdd(static_cast<Surface*>(surface));
		return surface;
	}
	/// Add a optional child element but it will not be displayed.
	void preAdd(Surface *surface);
	/// Adds a child element to the state.
	void add(Surface *surface);
	/// Adds a child element to the state.
	void add(Surface *surface, const std::string &id, const std::string &category, Surface *parent = 0);
	/// Gets whether the state is a full-screen.
	bool isScreen() const;
	/// Toggles whether the state is a full-screen.
	void toggleScreen();
	/// Initializes the state.
	virtual void init();
	/// Handles any events.
	virtual void handle(Action *action);
	/// Finds the active controller confirm/cancel button in a dialog.
	TextButton *getControllerButton(bool confirm) const;
	/// Checks for a mouse control at the given logical screen coordinates.
	bool isMouseTarget(double x, double y, Uint8 button) const;
	/// Activates one dialog button without broadcasting keyboard input.
	bool handleControllerButton(bool confirm, Action *action);
	/// Gets the visible focused text field selected by normal modal input routing.
	TextEdit *getFocusedTextEdit() const;
	/// Whether the current screen can accept interface navigation.
	virtual bool allowButtonNavigation() const;
	/// Routes focus to a nested screen such as an active interception window.
	virtual State *getNavigationState();
	virtual bool cycleNavigationState(bool backwards);
	bool isNavigationBoundary(bool backwards) const;
	bool isNavigationState() const;
	bool containsSurface(const Surface *surface) const;
	bool isNavigationEditing() const;
	/// Leaves only the selected control's interaction, keeping outer navigation active.
	bool cancelNavigationControl();
	/// Gets the currently selected, available interactive control.
	InteractiveSurface *getNavigationButton() const;
	/// Moves spatially, or cycles in reading order when dx and dy are zero.
	bool navigateButtons(int dx, int dy, bool backwards = false);
	/// Finishes control interaction and clears keyboard/controller selection.
	void clearButtonNavigation();
	/// Enters a control or activates its selection through normal input handlers.
	bool activateNavigationButton(Uint8 mouseButton = SDL_BUTTON_LEFT);
	/// Draws the selection border above the active state's surfaces.
	void blitButtonNavigation();
	/// Runs state functionality every cycle.
	virtual void think();
	/// Blits the state to the screen.
	virtual void blit();
	/// Hides all the state surfaces.
	void hideAll();
	/// Shows all the state surfaces.
	void showAll();
	/// Resets all the state surfaces.
	void resetAll();
	/// Get the localized text.
	LocalizedText tr(const std::string &id) const;
	/// Get the localized text.
	LocalizedText trAlt(const std::string &id, int alt) const;
	/// Get the localized text.
	LocalizedText tr(const std::string &id, unsigned n) const;
	/// Get the localized text.
	LocalizedText tr(const std::string &id, SoldierGender gender) const;
	/// redraw all the text-type surfaces.
	void redrawText();
	/// does the state only have one text list (to scroll)?
	bool hasOnlyOneScrollableTextList() const;
	/// center all surfaces relative to the screen.
	void centerAllSurfaces();
	/// lower all surfaces by half the screen height.
	void lowerAllSurfaces();
	/// switch the colours to use the battlescape palette.
	void applyBattlescapeTheme(const std::string& category);
	/// Sets game object pointer
	static void setGamePtr(Game* game);
	/// Gets game object pointer
	static Game* getGame() { return _game; }
	/// Sets a modal surface.
	void setModal(InteractiveSurface *surface);

	/// Changes a set of colors on the state's 8bpp palette.
	void setStatePalette(const SDL_Color *colors, int firstcolor = 0, int ncolors = 256);
	/// Changes a set of colors on the state's 8bpp palette of helper surfaces.
	void setModPalette();

	/// Changes the state's 8bpp palette with certain resources.
	void setStandardPalette(const std::string &palette, int backpals = -1);
	/// Changes the state's 8bpp palette with certain resources.
	void setCustomPalette(SDL_Color *colors, int cursorColor);

	/// Gets the state's 8bpp palette.
	SDL_Color *getPalette();

	/// Let the state know the window has been resized.
	virtual void resize(int &dX, int &dY);
	/// Re-orients all the surfaces in the state.
	virtual void recenter(int dX, int dY);

	/// Gets cursor X coordinate.
	int getCursorX() const;
	/// Gets cursor Y coordinate.
	int getCursorY() const;
};

}
