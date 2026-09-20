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
#include "MiniBaseView.h"
#include <cmath>
#include <algorithm>
#include "../Engine/SurfaceSet.h"
#include "../Engine/Action.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseFacility.h"
#include "../Mod/RuleBaseFacility.h"

namespace OpenXcom
{

bool MiniBaseView::isNavigationTarget()
{
	return _bases && !_bases->empty() && InteractiveSurface::isNavigationTarget();
}

SDL_Rect MiniBaseView::getNavigationRect(bool active) const
{
	if (!active)
		return InteractiveSurface::getNavigationRect(false);
	return { static_cast<Sint16>(getX() + _navigationBase * (MINI_SIZE + 2)),
		static_cast<Sint16>(getY()), MINI_SIZE + 2, static_cast<Uint16>(getHeight()) };
}

NavigationResult MiniBaseView::handleNavigation(NavigationCommand command, State *state)
{
	if (command == NavigationCommand::Cancel || command == NavigationCommand::End || !_bases || _bases->empty())
		return NavigationResult::Finished;
	const size_t count = std::min(_bases->size(), static_cast<size_t>(MAX_BASES));
	if (command == NavigationCommand::Begin)
		_navigationBase = std::min(_base, count - 1);
	else if (command == NavigationCommand::Left || command == NavigationCommand::Up)
		_navigationBase = (_navigationBase + count - 1) % count;
	else if (command == NavigationCommand::Right || command == NavigationCommand::Down)
		_navigationBase = (_navigationBase + 1) % count;
	else
		return NavigationResult::Unhandled;
	refreshNavigationHover(state);
	return NavigationResult::Handled;
}

/**
 * Sets up a mini base view with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
MiniBaseView::MiniBaseView(int width, int height, int x, int y) : InteractiveSurface(width, height, x, y), _bases(0), _texture(0), _base(0), _hoverBase(0), _red(0), _green(0), _blue(0)
{
}

/**
 *
 */
MiniBaseView::~MiniBaseView()
{
}

/**
 * Changes the current list of bases to display.
 * @param bases Pointer to base list to display.
 */
void MiniBaseView::setBases(std::vector<Base*> *bases)
{
	_bases = bases;
	_redraw = true;
}

/**
 * Changes the texture to use for drawing
 * the various base elements.
 * @param texture Pointer to SurfaceSet to use.
 */
void MiniBaseView::setTexture(SurfaceSet *texture)
{
	_texture = texture;
}

/**
 * Returns the base the mouse cursor is currently over.
 * @return ID of the base.
 */
size_t MiniBaseView::getHoveredBase() const
{
	return _hoverBase;
}

/**
 * Changes the base that is currently selected on
 * the mini base view.
 * @param base ID of base.
 */
void MiniBaseView::setSelectedBase(size_t base)
{
	_base = base;
	_redraw = true;
}

/**
 * Draws the view of all the bases with facilities
 * in varying colors.
 */
void MiniBaseView::draw()
{
	Surface::draw();
	for (size_t i = 0; i < MAX_BASES; ++i)
	{
		// Draw base squares
		if (i == _base)
		{
			SDL_Rect r;
			r.x = i * (MINI_SIZE + 2);
			r.y = 0;
			r.w = MINI_SIZE + 2;
			r.h = MINI_SIZE + 2;
			drawRect(&r, 1);
		}
		_texture->getFrame(41)->blitNShade(this, i * (MINI_SIZE + 2), 0);

		// Draw facilities
		if (i < _bases->size())
		{
			SDL_Rect r;
			lock();
			for (const auto* fac : *_bases->at(i)->getFacilities())
			{
				int color;
				if (fac->getDisabled())
					color = _blue;
				else if (fac->getBuildTime() == 0)
					color = _green;
				else
					color = _red;

				r.x = i * (MINI_SIZE + 2) + 2 + fac->getX() * 2;
				r.y = 2 + fac->getY() * 2;
				r.w = fac->getRules()->getSizeX() * 2;
				r.h = fac->getRules()->getSizeY() * 2;
				drawRect(&r, color+3);
				r.x++;
				r.y++;
				r.w--;
				r.h--;
				drawRect(&r, color+5);
				r.x--;
				r.y--;
				drawRect(&r, color+2);
				r.x++;
				r.y++;
				r.w--;
				r.h--;
				drawRect(&r, color+3);
				r.x--;
				r.y--;
				setPixel(r.x, r.y, color+1);
			}
			unlock();
		}
	}
}

/**
 * Selects the base the mouse is over.
 * @param action Pointer to an action.
 * @param state State that the action handlers belong to.
 */
void MiniBaseView::mouseOver(Action *action, State *state)
{
	_hoverBase = (int)floor(action->getRelativeXMouse() / ((MINI_SIZE + 2) * action->getXScale()));
	InteractiveSurface::mouseOver(action, state);
}

void MiniBaseView::setColor(Uint8 color)
{
	_green = color;
}
void MiniBaseView::setSecondaryColor(Uint8 color)
{
	_red = color;
}
void MiniBaseView::setBorderColor(Uint8 color)
{
	_blue = color;
}

}
