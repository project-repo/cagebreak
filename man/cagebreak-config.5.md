cagebreak-config(1) "Version 1.8.3" "Cagebreak Manual"

# NAME

*cagebreak-config* — Cagebreak config file

# SYNOPSIS

*\$XDG_CONFIG_PATH/cagebreak/config*

# DESCRIPTION

The cagebreak configuration file is a simple plain text file which configures
the way cagebreak behaves.
Each line constitutes a separate command which is parsed
independently from the rest of the file. Comments can be added
by prepending a line with the # symbol.

## COMMANDS

*abort*
	Return to the default mode without running any command

*background - Set background color*
	Set the background color. This command expects three floating point numbers
	between 0 and 1, specifying the r, g and b values respectively.
	(e.g. "background 1.0 0.0 0.0" sets to background color to red)
	There is no support for specifying a background image.

*bind - Bind key to command in root mode*
	This command requires a key (see *KEY DEFINITIONS*) and a command (see *COMMANDS*) as an argument.
	Subsequently, pressing this key while in command mode executes the
	supplied action. bind <key> <command> is equivalent to
		definekey root <key> <command>

*close - Close current window*
	This command closes the current window. It may be useful for windows of applications which
	do not offer any method of closing them.

*definekey - Bind key to action in arbitrary mode*
	This command behaves similarly to the *bind* command with the
	difference that the mode in which the keybinding is activated is
	specified by the user. A call to this function is to be structured as follows:

		definekey <mode> <key> <command>

*definemode*
	This command requires a single argument; the name of the mode to be defined.
	Subsequent to a call to this function, the defined mode may be used along with
	the definekey command to create a custom key mapping. Synopsis:

		definemode <mode>

*escape*
	Defines the key with which the current mode can be changed to "root".
	escape <key> is equivalent to definekey top <key> switch_mode root

*exchangedown*
	Exchange the current window with the window in the tile to the bottom

*exchangeleft*
	Exchange the current window with the window in the tile to the left

*exchangeright*
	Exchange the current window with the window in the tile to the right

*exchangeup*
	Exchange the current window with the window in the tile to the top

*exec*
	Executes the supplied shell command using *sh -c "<command>"*. Synopsis:

		exec <command>

*focus*
	Focus next tile

*focusdown*
	Focus the tile to the bottom

*focusleft*
	Focus the tile to the left

*focusprev*
	Focus previous tile

*focusright*
	Focus the tile to the right

*focusup*
	Focus the tile to the top

*hsplit*
	Split current tile horizontally

*input <identifier> <setting> <value>*
	Set the setting "<setting>" to "<value>" for device "<identifier>". The identifier can either be "\*" (wildcard), of the form "type:<device_type>" or the identifier of the device as printed for example by *cagebreak -s*. The supported input types are
	- touchpad
	- pointer
	- keyboard
	- touch
	- tablet_tool
	- tablet_pad
	- switch

	Configurations are applied sequentially. Currently, only libinput devices may be configured. The available settings and their corresponding values are as follows:

	*accel_profile adaptive|flat*
		Sets the pointer acceleration profile for the specified input device.

	*calibration_matrix <6 space-separated floating point values>*
		Sets the calibration matrix.

	*click_method none|button_areas|clickfinger*
		Changes the click method for the specified device.

	*drag enabled|disabled*
		Enables or disables tap-and-drag for specified input device.

	*drag_lock enabled|disabled*
		Enables or disables drag lock for specified input device.

	*dwt enabled|disabled*
		Enables or disables disable-while-typing for the specified input device.

	*enabled|disabled|disabled_on_external_mouse*
		Enables or disables send_events for specified input device. Disabling
		send_events disables the input device.

	*left_handed enabled|disabled*
		Enables or disables left handed mode for specified input device.

	*middle_emulation enabled|disabled*
		Enables or disables middle click emulation.

	*natural_scroll enabled|disabled*
		Enables or disables natural (inverted) scrolling for the specified input
		device.

	*pointer_accel [<-1|1>]*
		Changes the pointer acceleration for the specified input device.

	*scroll_button disable|<event-code-or-name>*
		Sets the button used for scroll_method on_button_down. The button can
		be given as an event name or code, which can be obtained from *libinput
		debug-events*. If set to
		_disable_, it disables the scroll_method on_button_down.

	*scroll_factor <floating point value>*
		Changes the scroll factor for the specified input device. Scroll speed will
		be scaled by the given value, which must be non-negative.

	*scroll_method none|two_finger|edge|on_button_down*
		Changes the scroll method for the specified input device.

	*tap enabled|disabled*
		Enables or disables tap for specified input device.

	*tap_button_map lrm|lmr*
		Specifies which button mapping to use for tapping. _lrm_ treats 1 finger as
		left click, 2 fingers as right click, and 3 fingers as middle click. _lmr_
		treats 1 finger as left click, 2 fingers as middle click, and 3 fingers as
		right click.

*mode <mode>*
	Enter mode "<mode>". After a keybinding is processed, return to default mode

*movetonextscreen*
	Move the current window to the next screen

*movetoworkspace <n>*
	Move the currently focused window to the n-th workspace

*next*
	Focus next window in current tile

*nextscreen*
	Focus the next screen

*only*
	Remove all splits and make the current window fill the entire screen

*output <name> [[pos <xpos> <ypos> res <width>x<height> rate <rate>] | enable | disable]*
	Configure the output "<name>". <xpos> and <ypos> are the position of the monitor
	in pixels. The top-left monitor should have the coordinates 0 0. <width> and
	<height> specify the resolution in pixels and <rate> sets the refresh rate of
	the monitor (often this is 50 or 60). The options enable and disable
	enable or disable the output <name>. Note that if <output> is the only enabled
	output, *output <output> disable* has no effect.

*prev*
	Focus previous window in current tile

*prevscreen*
	Focus the previous screen

*quit*
	Exit cagebreak

*resizedown*
	Resize the current tile towards the bottom

*resizeleft*
	Resize the current tile towards the left

*resizeright*
	Resize the current tile towards the right

*resizeup*
	Resize the current tile towards the top

*show_info*
	Display info about the current setup. In particular, print the identifiers
	of the available inputs and outputs.

*setmode <mode>*
	Set the default mode to <mode>

*switchvt <n>*
	Switch to tty n

*time*
	Display time

*vsplit*
	Split current tile vertically

*workspace <n>*
	Change to the n-th workspace

*workspaces*
	Requires a single integer larger than 1 and less than 30 as an argument. Sets the number of
	workspaces to the supplied number

# MODES

By default, three modes are defined:

*top*
	The default mode. Keybindings defined in this mode can be accessed
	directly, without the use of an escape key.

*root*
	The command mode. Keybindings defined in this mode can be accessed
	after pressing the key defined by the *escape* command.

*resize*
	Resize mode. This mode is used for resizing tiles.

# KEY DEFINITIONS

Keys are specified by their names as displayed for example by *xev*.
In addition, modifiers can be specified using the following syntax:

	<mod>-<key>

The supported modifiers are:

*A - Alt*

*C - Control*

*L - Logo*

*S - Shift*

*2 - Mod2*

*3 - Mod 3*

*5 - Mod 5*

For example to specify the keybinding Control+t, the expression:

	C-t

is used.

# SEE ALSO

*cagebreak(1)*

# BUGS

See GitHub Issues: <https://github.com/project-repo/cagebreak/issues>

# LICENSE

Copyright (c) 2020-2022 The Cagebreak authors
Copyright (c) 2018-2020 Jente Hidskes
Copyright (c) 2019 The Sway authors

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
