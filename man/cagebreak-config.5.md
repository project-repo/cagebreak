cagebreak-config(5) "Version 1.8.2" "Cagebreak Manual"

# NAME

*cagebreak-config* — Cagebreak configuration file

# SYNOPSIS

*\$XDG_CONFIG_PATH/cagebreak/config*

# DESCRIPTION

The cagebreak configuration is a plain text file.

Each line consists of a comment or a command and its arguments which
are parsed sequentially but independently from the rest of the file.

Each line starting with a "#" is a comment.

See *KEY DEFINITIONS* for details on modifier keys and *MODES* for details
on modes.

## COMMANDS

*abort*
	Return to default mode

*background <r> <g> <b>*
	Set RGB of background - <[r|g|b]> are floating point numbers
	between 0 and 1.
	There is no support for background images.

```
# Set background to red
background 1.0 0.0 0.0
```

*bind <key> <command>*
	Bind <key> to execute <command> if pressed in root mode

```
bind <key> <command>
# is equivalent to
definekey root <key> <command>
```

*close*
	Close current window - This may be useful for windows of
	applications which do not offer any method of closing them.

*definekey <mode> <key> <command>*
	Bind <key> to execute <command> if pressed in <mode> -
	*definekey* is a more general version of *bind*.

*definemode <mode>*
	Define new mode <mode> - After a call to *definemode*,
	<mode> can be used with *definekey* to create a custom key mapping.

```
# define new mode and create a mapping for it
definemode foo
definekey foo C-t abort
```

*escape <key>*
	Set <key> to switch to root mode to execute one command

```
escape <key>
# is equivalent to
definekey top <key> mode root
```

*exchangedown*
	Exchange current window with window in the tile to the bottom

*exchangeleft*
	Exchange current window with window in the tile to the left

*exchangeright*
	Exchange current window with window in the tile to the right

*exchangeup*
	Exchange current window with window in the tile to the top

*exec <command>*
	Execute <command> using *sh -c*

*focus*
	Focus next tile

*focusdown*
	Focus tile to the bottom

*focusleft*
	Focus tile to the left

*focusprev*
	Focus previous tile

*focusright*
	Focus tile to the right

*focusup*
	Focus tile to the top

*hsplit*
	Split current tile horizontally

*input <identifier> <setting> <value>*
	Set <setting> to <value> for device <identifier> -
	<identifier> can be "\*" (wildcard), of the form
	"type:<device_type>" or the identifier of the device as printed
	for example by *cagebreak -s*. The supported input types are
	- touchpad
	- pointer
	- keyboard
	- touch
	- tablet_tool
	- tablet_pad
	- switch

	Configurations are applied sequentially. Currently, only libinput
	devices may be configured. The available settings and their
	corresponding values are as follows:

	*accel_profile adaptive|flat*
		Set pointer acceleration profile for specified input device

	*calibration_matrix <6 space-separated floating point values>*
		Set calibration matrix

	*click_method none|button_areas|clickfinger*
		Change click method for the specified device

	*drag enabled|disabled*
		Enable or disable tap-and-drag for specified input device

	*drag_lock enabled|disabled*
		Enable or disable drag lock for specified input device

	*dwt enabled|disabled*
		Enable or disable disable-while-typing for specified input
		device

	*enabled|disabled|disabled_on_external_mouse*
		Enable or disable send_events for specified input device -
		Disabling send_events disables the input device.

	*left_handed enabled|disabled*
		Enable or disable left handed mode for specified input device

	*middle_emulation enabled|disabled*
		Enable or disable middle click emulation

	*natural_scroll enabled|disabled*
		Enable or disable natural (inverted) scrolling for specified
		input device

	*pointer_accel [<-1|1>]*
		Change the pointer acceleration for specified input device

	*scroll_button disable|<event-code-or-name>*
		Set button used for scroll_method on_button_down - The button
		can be given as an event name or code, which can be obtained from
		*libinput debug-events*. If set to _disable_, it disables the
		scroll_method on_button_down.

	*scroll_factor <floating point value>*
		Change the scroll factor for the specified input device - Scroll
		speed will be scaled by the given value, which must be non-negative.

	*scroll_method none|two_finger|edge|on_button_down*
		Change scroll method for specified input device

	*tap enabled|disabled*
		Enable or disable tap for specified input device

	*tap_button_map lrm|lmr*
		Specify which button mapping to use for tapping - _lrm_ treats 1
		finger as left click, 2 fingers as right click, and 3 fingers as
		middle click. _lmr_ treats 1 finger as left click, 2 fingers as
		middle click, and 3 fingers as right click.

*mode <mode>*
	Enter mode "<mode>" - Returns to default mode, after a command is
	executed.

*movetonextscreen*
	Move current window to next screen

*movetoprevscreen*
	Move current window to previous screen

*movetoscreen <n>*
	Move currently focused window to <n>-th screen

*movetoworkspace <n>*
	Move currently focused window to <n>-th workspace

*next*
	Focus next window in current tile

*nextscreen*
	Focus next screen

*only*
	Remove all splits and make current window fill the entire screen

*output <name> [[pos <xpos> <ypos> res <width>x<height> rate <rate>] | enable | disable | prio <n> ]*
	Configure output "<name>" -
	- <xpos> and <ypos> are the position of the
	  monitor in pixels. The top-left monitor should have the coordinates 0 0.
	- <width> and <height> specify the resolution in pixels.
	- <rate> sets the refresh rate of the monitor (often this is 50 or 60).
	- enable and disable enable or disable <name>. Note that if
	  <output> is the only enabled output, *output <output> disable* has
	  no effect.
	- prio <n> is used to set the priority of an output. If
	  nothing else is set, outputs are added as they request to be added
	  and have a numerical priority of -1. Using prio <n> it is possible
	  to set priorities for outputs, where <n> >= 1. The larger <n> is,
	  the higher the priority is, that is to say, the earlier the output
	  will appear in the list of outputs.

*prev*
	Focus previous window in current tile

*prevscreen*
	Focus previous screen

*quit*
	Exit cagebreak

*resizedown*
	Resize current tile towards the bottom

*resizeleft*
	Resize current tile towards the left

*resizeright*
	Resize current tile towards the right

*resizeup*
	Resize current tile towards the top

*screen <n>*
	Change to <n>-th screen

*show_info*
	Display information about the current setup - In particular, print the identifiers
	of the available inputs and outputs.

*setmode <mode>*
	Set default mode to <mode>

*switchvt <n>*
	Switch to tty <n>

*time*
	Display time

*vsplit*
	Split current tile vertically

*workspace <n>*
	Change to <n>-th workspace

*workspaces <n>*
	Set number of workspaces to <n> - <n> is a single integer larger than 1
	and less than 30.

# MODES

By default, three modes are defined:

*top*
	Default mode - Keybindings defined in this mode can be accessed
	directly.
	- *definekey* can be used to set keybindings for top mode.
	- *setmode* can be used to set a different default mode.

*root*
	Command mode - Keybindings defined in this mode can be accessed
	after pressing the key defined by *escape*.
	- *bind* can be used to set keybindings for root mode.

*resize*
	Resize mode - Used to resize tiles.

*definemode* can be used to create additional modes.

# KEY DEFINITIONS

Keys are specified by their names as displayed for example by *xev*.

Modifiers can be specified using the following syntax:

	<mod>-<key>

The supported modifiers are:

*A - Alt*

*C - Control*

*L - Logo*

*S - Shift*

*2 - Mod2*

*3 - Mod 3*

*5 - Mod 5*

For example to specify the keybinding Control+t, the expression

```
C-t
```

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
