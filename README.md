# OpenXcom [![Workflow Status][workflow-badge]][actions-url]

[workflow-badge]: https://github.com/OpenXcom/OpenXcom/workflows/ci/badge.svg
[actions-url]: https://github.com/OpenXcom/OpenXcom/actions

OpenXcom is an open-source clone of the popular "UFO: Enemy Unknown" ("X-COM:
UFO Defense" in the USA release) and "X-COM: Terror From the Deep" videogames
by Microprose, licensed under the GPL and written in C++ / SDL.

See more info at the [website](https://openxcom.org)
and the [wiki](https://www.ufopaedia.org/index.php/OpenXcom).

Uses modified code from SDL\_gfx (LGPL) with permission from author.

## Installation

OpenXcom requires a vanilla copy of the X-COM resources -- from either or both
of the original games.  If you own the games on Steam, the Windows installer
will automatically detect it and copy the resources over for you.

If you want to copy things over manually, you can find the Steam game folders
at:

    UFO: "Steam\SteamApps\common\XCom UFO Defense\XCOM"
    TFTD: "Steam\SteamApps\common\X-COM Terror from the Deep\TFD"

Do not use modded versions (e.g. with XcomUtil) as they may cause bugs and
crashes.  Copy the UFO subfolders to the UFO subdirectory in OpenXcom's data
or user folder and/or the TFTD subfolders to the TFTD subdirectory in OpenXcom's
data or user folder (see below for folder locations).

## Mods

Mods are an important and exciting part of the game.  OpenXcom comes with a set
of standard mods based on traditional XcomUtil and UFOExtender functionality.
There is also a [mod portal website](https://openxcom.mod.io/) with a thriving
mod community with hundreds of innovative mods to choose from.

To install a mod, go to the mods subdirectory in your user directory (see below
for folder locations).  Extract the mod into a new subdirectory.  WinZip has an
"Extract to" option that creates a directory whose name is based on the archive
name.  It doesn't really matter what the directory name is as long as it is
unique.  Some mods are packed with extra directories at the top, so you may
need to move files around inside the new mod directory to get things straighted
out.  For example, if you extract a mod to mods/LulzMod and you see something
like:

    mods/LulzMod/data/TERRAIN/
    mods/LulzMod/data/Rulesets/

and so on, just move everything up a level so it looks like:

    mods/LulzMod/TERRAIN/
    mods/LulzMod/Rulesets/

and you're good to go!  Enable your new mod on the Options -> Mods page in-game.

## Directory Locations

OpenXcom has three directory locations that it searches for user and game files:

<table>
  <tr>
    <th>Folder Type</th>
    <th>Folder Contents</th>
  </tr>
  <tr>
    <td>user</td>
    <td>mods, savegames, screenshots</td>
  </tr>
  <tr>
    <td>config</td>
    <td>game configuration</td>
  </tr>
  <tr>
    <td>data</td>
    <td>UFO and TFTD data files, standard mods, common resources</td>
  </tr>
</table>

Each of these default to different paths on different operating systems (shown
below).  For the user and config directories, OpenXcom will search a list of
directories and use the first one that already exists.  If none exist, it will
create a directory and use that.  When searching for files in the data
directory, OpenXcom will search through all of the named directories, so some
files can be installed in one directory and others in another.  This gives
you some flexibility in case you can't copy UFO or TFTD resource files to some
system locations.  You can also specify your own path for each of these by
passing a commandline argument when running OpenXcom.  For example:

    openxcom -data "$HOME/bin/OpenXcom/usr/share/openxcom"

or, if you have a fully self-contained installation:

    openxcom -data "$HOME/games/openxcom/data" -user "$HOME/games/openxcom/user" -config "$HOME/games/openxcom/config"

### Windows

User and Config folder:
- C:\Documents and Settings\\\<user\>\My Documents\OpenXcom (Windows 2000/XP)
- C:\Users\\\<user\>\Documents\OpenXcom (Windows Vista/7)
- \<game directory\>\user
- .\user

Data folders:
- C:\Documents and Settings\\\<user\>\My Documents\OpenXcom\data (Windows 2000/XP)
- DATADIR build flag
- C:\Users\\\<user\>\Documents\OpenXcom\data (Windows Vista/7/8)
- \<game directory\>
- . (the current directory)

### Mac OS X

User and Config folder:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/Library/Application Support/OpenXcom
- $HOME/.openxcom
- ./user

Data folders:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/Library/Application Support/OpenXcom (if $XDG\_DATA\_HOME is not defined)
- DATADIR build flag
- $XDG\_DATA\_DIRS/openxcom (for each directory in $XDG\_DATA\_DIRS if $XDG\_DATA\_DIRS is defined)
- /Users/Shared/OpenXcom (if $XDG\_DATA\_DIRS is not defined or is empty)
- . (the current directory)

### Linux

User folder:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/.local/share/openxcom (if $XDG\_DATA\_HOME is not defined)
- $HOME/.openxcom
- ./user

Config folder:
- $XDG\_CONFIG\_HOME/openxcom (if $XDG\_CONFIG\_HOME is defined)
- $HOME/.config/openxcom (if $XDG\_CONFIG\_HOME is not defined)

Data folders:
- $XDG\_DATA\_HOME/openxcom (if $XDG\_DATA\_HOME is defined)
- $HOME/.local/share/openxcom (if $XDG\_DATA\_HOME is not defined)
- DATADIR build flag
- $XDG\_DATA\_DIRS/openxcom (for each directory in $XDG\_DATA\_DIRS if $XDG\_DATA\_DIRS is defined)
- /usr/local/share/openxcom (if $XDG\_DATA\_DIRS is not defined or is empty)
- /usr/share/openxcom (if $XDG\_DATA\_DIRS is not defined or is empty)
- the directory data files were installed to
- . (the current directory)

## Configuration

OpenXcom has a variety of game settings and extras that can be customized, both
in-game and out-game. These options are global and affect any old or new
savegame.

For more details please check the [wiki](https://ufopaedia.org/index.php/Options_(OpenXcom)).

### Joystick controls

The left stick moves the shared mouse cursor. A clicks an interactive control
under the cursor, including text fields, lists, sliders and their arrows. When
no control is under the cursor, A activates the dialog's available OK shortcut;
otherwise it acts as a left mouse click. B activates the dialog's available
Cancel shortcut, or acts as a right mouse click. LB and RB always click at the
cursor, including when a dialog has OK or Cancel shortcuts. Physical mouse and
keyboard input continue to work alongside the controller.

Any button or D-pad direction on the enabled controller skips movie playback,
including the startup intro and its closing pause or fade. Stick motion does
not skip movies.

Edit these bindings under `options:` in `options.cfg` while the game is closed:

```yaml
  oxceJoystickButtonOk: 0          # A: click a control, otherwise OK/left click
  oxceJoystickButtonCancel: 1      # B: Cancel, otherwise right click
  oxceJoystickButtonLeftClick: 4   # LB: left click at the cursor
  oxceJoystickButtonRightClick: 5  # RB: right click at the cursor
  oxceJoystickButtonKeyboard: 3    # Y: toggle the virtual keyboard
  oxceJoystickButtonDelete: 2      # X: delete in a focused text field or keyboard
```

Button indices are zero-based and depend on the controller. Use `-1` to disable
a binding; values outside `0..255` or the controller's button range have no
effect. Dialog actions use the existing `keyOk` and `keyCancel` settings
(Enter and Escape by default), including any changes made in the keyboard
controls settings.

With `keyboardMode: 2`, Y toggles the virtual keyboard for a focused text field.
There, A selects a key, B closes the keyboard and X deletes a character. Outside
the virtual keyboard, X deletes the character before the caret in a focused
text field when `keyboardMode` is `1` or `2`; otherwise it sends Space. Outside
virtual-keyboard mode Y sends the configured OK shortcut. Buttons 6 through 9
retain the configured Cancel shortcut unless assigned another action above.

Closing the keyboard with Y or B preserves the text and keeps the field focused.
Use the dialog's OK control to confirm the text after closing the keyboard.
Holding a D-pad direction in the virtual keyboard repeats navigation after
400 ms, then every 100 ms. Releasing it, changing screens, or losing window
focus stops the repeat. Confirm, cancel and character input are not repeated.

If bindings share a button, virtual-keyboard toggle takes priority in virtual
keyboard mode, followed by explicit mouse clicks, OK, Cancel, then Delete.
Left click takes priority if both mouse bindings share a button. Assign distinct
buttons to keep every action accessible.

### Virtual keyboard layouts

Keyboard layouts are hidden settings under `options:` in `options.cfg`. Close
the game before editing the file. This default reproduces the QWERTY keyboard:

```yaml
  oxceVirtualKeyboardLanguages:
    - en-US
  oxceVirtualKeyboardLayouts:
    en-US:
      label: EN
      normal: >-
        ` 1 2 3 4 5 6 7 8 9 0 - = ||
        q w e r t y u i o p [ ] \ ||
        a s d f g h j k l ; ' ||
        z x c v b n m , . / || shift space
      shifted: >-
        ~ ! @ # $ % ^ & * ( ) _ + ||
        Q W E R T Y U I O P { } | ||
        A S D F G H J K L : " ||
        Z X C V B N M < > ? || shift space
```

Separate keys with whitespace and rows with `||`; a final `||` is optional.
Each key is one character or one of the lowercase tokens `backspace`, `shift`
and `space`. The `normal` and `shifted` strings must have matching rows and key
positions, with special tokens in the same positions. `normal` is required;
an omitted or empty `shifted` uses `normal`, and an omitted or empty `label`
uses the layout ID. Folded YAML strings (`>-`) let punctuation such as `#`,
quotes and backslashes remain literal without extra escaping.

The first row sets the grid width. Other rows are centered within it, and
`space` expands to fill the remaining row width; multiple space keys share
that width. Backspace, Shift and Space have a minimum width of two ordinary
key positions. Rows must fit within the first row, with at most 14 ordinary
key positions across and eight rows. There are no on-screen OK or Esc keys;
B or Y closes the keyboard, then the dialog's OK control confirms the text.
The default layout has one Shift key beside Space in the bottom row, with
the letter rows centered above it. There is no Backspace key; use X to delete.
Custom layouts can still include `backspace`.

Add another ID under `oxceVirtualKeyboardLayouts` and include it in
`oxceVirtualKeyboardLanguages` to enable it without rebuilding. Valid layouts
are enabled in that list's order; duplicate IDs are ignored. With more than
one enabled layout, the language label becomes a button: click it to cycle
through the layouts and wrap to the first one, or move up from the first key
row with the D-pad and press A. Switching preserves the text, caret position
and Shift state. The compact language button shows the first two characters
of the layout label (for example, EN or RU), without a Shift marker. With one
layout, the label is informational.

The language control's right edge follows the last key in the first row,
and the current text begins above that row's first key. The window follows
the row width with three quarters of a letter key of padding on each side,
rounded to a whole logical pixel, and the same padding above and below.
Vertical padding is reduced slightly for eight rows to keep the window on
screen. These distances use the game's logical interface coordinates and
scale with the rest of the UI at each resolution. Very narrow custom layouts
hide the text preview when there is no room beside the language control.

Unknown IDs remain in the preference list. Missing settings are filled with
the default `en-US` layout; malformed entries are ignored, and if none of the
requested layouts is usable the built-in QWERTY layout is used. The keyboard
layout is independent of the game's language; custom characters still need
to be present in its fonts.

## Development

OpenXcom requires the following developer libraries:

- [SDL](https://www.libsdl.org) (libsdl1.2)
- [SDL\_mixer](https://www.libsdl.org/projects/SDL_mixer/) (libsdl-mixer1.2)
- [SDL\_gfx](https://www.ferzkopp.net/wordpress/2016/01/02/sdl_gfx-sdl2_gfx/) (libsdl-gfx1.2), version 2.0.22 or later
- [SDL\_image](https://www.libsdl.org/projects/SDL_image/) (libsdl-image1.2)

The source code includes files for the following build tools:

- Microsoft Visual C++ 2010 or newer
- Xcode
- Make (see Makefile.simple)
- CMake

It's also been tested on a variety of other tools on Windows/Mac/Linux. More
detailed compiling instructions are available at the
[wiki](https://ufopaedia.org/index.php/Compiling_(OpenXcom)), along with
pre-compiled dependency packages.
