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
#include "MedikitView.h"
#include <algorithm>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Engine/SurfaceSet.h"
#include "../Engine/Action.h"
#include "../Engine/Language.h"
#include "../Savegame/BattleUnit.h"
#include "../Interface/Text.h"

namespace OpenXcom
{

bool MedikitView::isNavigationTarget()
{
	return isNavigationEnabled() && _visible && !_hidden && _isFocused && _unit;
}

SDL_Rect MedikitView::getNavigationRect(bool active) const
{
	if (!active || _selectedPart < 0)
		return InteractiveSurface::getNavigationRect(false);
	Surface *part = _game->getMod()->getSurfaceSet("MEDIBITS.DAT")->getFrame(_selectedPart);
	int left = part->getWidth(), top = part->getHeight(), right = -1, bottom = -1;
	for (int y = 0; y < part->getHeight(); ++y)
		for (int x = 0; x < part->getWidth(); ++x)
			if (part->getPixel(x, y))
			{
				left = std::min(left, x); top = std::min(top, y);
				right = std::max(right, x); bottom = std::max(bottom, y);
			}
	if (right < left)
		return InteractiveSurface::getNavigationRect(false);
	return { static_cast<Sint16>(getX() + left), static_cast<Sint16>(getY() + top),
		static_cast<Uint16>(right - left + 1), static_cast<Uint16>(bottom - top + 1) };
}

NavigationResult MedikitView::handleNavigation(NavigationCommand command, State *)
{
	if (command == NavigationCommand::Cancel || command == NavigationCommand::End)
		return NavigationResult::Finished;
	const int count = std::min<int>(BODYPART_MAX, static_cast<int>(_game->getMod()->getSurfaceSet("MEDIBITS.DAT")->getTotalFrames()));
	if (count == 0)
		return NavigationResult::Finished;
	_selectedPart = std::max(0, std::min(_selectedPart, count - 1));
	if (command == NavigationCommand::Left || command == NavigationCommand::Up)
		_selectedPart = (_selectedPart + count - 1) % count;
	else if (command == NavigationCommand::Right || command == NavigationCommand::Down)
		_selectedPart = (_selectedPart + 1) % count;
	_redraw = true;
	return NavigationResult::Handled;
}
/**
 * Initializes the Medikit view.
 * @param w The MinikitView width.
 * @param h The MinikitView height.
 * @param x The MinikitView x origin.
 * @param y The MinikitView y origin.
 * @param game Pointer to the core game.
 * @param unit The wounded unit.
 * @param partTxt A pointer to a Text. Will be updated with the selected body part.
 * @param woundTxt A pointer to a Text. Will be updated with the amount of fatal wound.
 */
MedikitView::MedikitView (int w, int h, int x, int y, Game * game, BattleUnit *unit, Text *partTxt, Text *woundTxt) : InteractiveSurface(w, h, x, y), _game(game), _selectedPart(0), _unit(unit), _partTxt(partTxt), _woundTxt(woundTxt)
{
	updateSelectedPart();
	_redraw = true;
}

/**
 * Draws the medikit view.
 */
void MedikitView::draw()
{
	SurfaceSet *set = _game->getMod()->getSurfaceSet("MEDIBITS.DAT");
	int fatal_wound = _unit->getFatalWound((UnitBodyPart)_selectedPart);
	std::ostringstream ss, ss1;
	int green = 0;
	int red = 3;
	if (_game->getMod()->getInterface("medikit", false) && _game->getMod()->getInterface("medikit")->getElementOptional("body"))
	{
		green = _game->getMod()->getInterface("medikit")->getElement("body")->color;
		red = _game->getMod()->getInterface("medikit")->getElement("body")->color2;
	}
	this->lock();
	for (unsigned int i = 0; i < set->getTotalFrames(); i++)
	{
		int wound = _unit->getFatalWound((UnitBodyPart)i);
		Surface * surface = set->getFrame (i);
		int baseColor = wound ? red : green;
		surface->blitNShade(this, 0, 0, 0, false, baseColor);
	}
	this->unlock();

	_redraw = false;
	if (_selectedPart == -1)
	{
		return;
	}
	ss << _game->getLanguage()->getString(PARTS_STRING[_selectedPart]);
	ss1 << fatal_wound;
	_partTxt->setText(ss.str());
	_woundTxt->setText(ss1.str());
}

/**
 * Handles clicks on the medikit view.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void MedikitView::mouseClick (Action *action, State *)
{
	SurfaceSet *set = _game->getMod()->getSurfaceSet("MEDIBITS.DAT");
	int x = action->getRelativeXMouse() / action->getXScale();
	int y = action->getRelativeYMouse() / action->getYScale();
	for (unsigned int i = 0; i < set->getTotalFrames(); i++)
	{
		Surface * surface = set->getFrame (i);
		if (surface->getPixel(x, y))
		{
			_selectedPart = i;
			_redraw = true;
			break;
		}
	}
}

/**
 * Gets the selected body part.
 * @return The selected body part.
 */
int MedikitView::getSelectedPart() const
{
	return _selectedPart;
}

/**
 * Updates the selected body part.
 * If there is a wounded body part, selects that.
 * Otherwise does not change the selected part.
 */
void MedikitView::updateSelectedPart()
{
	for (int i = 0; i < BODYPART_MAX; ++i)
	{
		if (_unit->getFatalWound((UnitBodyPart)i))
		{
			_selectedPart = i;
			break;
		}
	}
}

}
