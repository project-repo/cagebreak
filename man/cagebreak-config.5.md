cagebreak-config(5) "Version 2.3.2" "Cagebreak Manual"

# NAME

*cagebreak-config* Cagebreak configuration file

# SYNOPSIS

*\$XDG_CONFIG_PATH/cagebreak/config*

# DESCRIPTION

The cagebreak configuration is a plain text file.

Each line consists of a comment or a command and its arguments which
are parsed sequentially but independently from the rest of the file.

Each line starting with a "#" is a comment.

Note that nesting of commands is limited to 50 times.
Lines are arbitrarily long in practice, though a reasonable limit of 4Mb has been set.

See *KEY DEFINITIONS* for details on modifier keys and *MODES* for details
on modes.

## COMMANDS

*abort*
	Return to default mode

*background <r\> <g\> <b\>*
	Set RGB of background - <[r|g|b]\> are floating point numbers
	between 0 and 1.
	There is no support for background images.

```
# Set background to red
background 1.0 0.0 0.0
```

*bind <key\> <command\>*
	Bind <key\> to execute <command\> if pressed in root mode

```
bind <key> <command>
# is equivalent to
definekey root <key> <command>
```

*close*
	Close current window - This may be useful for windows of
	applications which do not offer any method of closing them.

*configure_message [font <font description\>|[f|b]g_color <r\> <g\> <b\> <a\>|display_time <n\>|anchor <position\>]*
	Configure message characteristics -
	- font <font description\> sets the font of the message.
	  Here, <font description\> is either
	    - an X core font description or
	    - a FreeType font description via pango
	- fg_color <r\> <g\> <b\> <a\> sets the RGBA of the foreground
	- bg_color <r\> <g\> <b\> <a\> sets the RGBA of the background
	- display_time <n\> sets the display time in seconds
	- anchor <position\> sets the position of the message.
      <position\> may be one of {top,bottom}\_{left,center,right} or center.

```
# Set font
## Set example X code font
configure_message font -misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1
## Set example FreeType font description
configure_message font pango:monospace 10

# Set foreground RGBA to red
configure_message fg_color 1.0 0.0 0.0 1.0

# Set background RGBA to red
configure_message bg_color 1.0 0.0 0.0 1.0

# Set duration for message display to four seconds
configure_message display_time 4
```

*cursor [enable|disable]*
	Enable or disable cursor

	This simply hides the cursor. Pointing and clicking is
	still possible.

*custom_event <message\>*
	Send a custom event to the IPC socket

	This sends an event of type "custom_event" to all programs
	listening to the IPC socket along with the string <message\>.
	See *cagebreak-socket(7)* for more details.

*definekey <mode\> <key\> <command\>*
	Bind <key\> to execute <command\> if pressed in <mode\> -
	*definekey* is a more general version of *bind*.

*definemode <mode\>*
	Define new mode <mode\> - After a call to *definemode*,
	<mode\> can be used with *definekey* to create a custom key mapping.

```
# define new mode and create a mapping for it
definemode foo
definekey foo C-t abort
```

*dump*
	Triggers the *dump* event, see *cagebreak-socket(7)* for details

*escape <key\>*
	Set <key\> to switch to root mode to execute one command

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

*exec <command\>*
	Execute <command\> using *sh -c*

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

*input <identifier\> <setting\> <value\>*
	Set <setting\> to <value\> for device <identifier\> -
	<identifier\> can be "\*" (wildcard), of the form
	"type:<device_type\>" or the identifier of the device as printed
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

	*calibration_matrix <6 space-separated floating point values\>*
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

	*events enabled|disabled|disabled_on_external_mouse*
		Enable or disable send_events for specified input device -
		Disabling send_events disables the input device.

	*keybindings [enabled|disabled]*
		Enable or disable keybinding interpretation for a specific keyboard.

	*left_handed enabled|disabled*
		Enable or disable left handed mode for specified input device

	*middle_emulation enabled|disabled*
		Enable or disable middle click emulation

	*natural_scroll enabled|disabled*
		Enable or disable natural (inverted) scrolling for specified
		input device

	*pointer_accel [<-1|1\>]*
		Change the pointer acceleration for specified input device

	*scroll_button disable|<event-code-or-name\>*
		Set button used for scroll_method on_button_down - The button
		can be given as an event name or code, which can be obtained from
		*libinput debug-events*. If set to _disable_, it disables the
		scroll_method on_button_down.

	*scroll_factor <floating point value\>*
		Change the scroll factor for the specified input device - Scroll
		speed will be scaled by the given value, which must be non-negative.

	*scroll_method none|two_finger|edge|on_button_down*
		Change scroll method for specified input device

	*repeat_delay <n\>*
		Repeat delay in ms for keyboards only

	*repeat_rate <n\>*
		Repeat rate in 1/s for keyboards only

	*tap enabled|disabled*
		Enable or disable tap for specified input device

	*tap_button_map lrm|lmr*
		Specify which button mapping to use for tapping - _lrm_ treats 1
		finger as left click, 2 fingers as right click, and 3 fingers as
		middle click. _lmr_ treats 1 finger as left click, 2 fingers as
		middle click, and 3 fingers as right click.

message <text\>
	Display a line of arbitrary text.

*mode <mode\>*
	Enter mode "<mode\>" - Returns to default mode, after a command is
	executed.

*movetonextscreen*
	Move currently focused window to next screen
	See *output* for differences between screen and output.

*movetoprevscreen*
	Move currently focused window to previous screen
	See *output* for differences between screen and output.

*movetoscreen <n\>*
	Move currently focused window to <n\>-th screen
	See *output* for differences between screen and output.

*movetoworkspace <n\>*
	Move currently focused window to <n\>-th workspace
	See *output* for differences between screen and output.

*next*
	Focus next window in current tile

*nextscreen*
	Focus next screen
	See *output* for differences between screen and output.

*only*
	Remove all splits and make current window fill the entire screen

*output <name\> [[pos <xpos\> <ypos\> res <width\>x<height\> rate <rate\> [scale <scale\>]] | enable | disable | [permanent|peripheral] | prio <n\> | rotate <n\>]*
	Configure output "<name\>" -
	- <xpos\> and <ypos\> are the position of the
	  monitor in pixels. The top-left monitor should have the coordinates 0 0.
	- <width\> and <height\> specify the resolution in pixels.
	- <rate\> sets the refresh rate of the monitor (often this is 50 or 60).
	- <scale\> sets the output scale (default is 1.0)
	- enable and disable enable or disable <name\>. Note that if
	  <output\> is the only enabled output, *output <output\> disable* has
	  no effect.
	- permanent sets <name\> to persist even on disconnect. When
	  the physical monitor is disconnected, the output is
	  maintained and operates identically to the attached monitor. On reconnect,
	  the monitor operates as though it was never disconnected. Setting the
	  output role to peripheral when the monitor is disconnected,
	  destroys the output, as if the monitor were disconnected.
	- peripheral sets the role of <name\> to peripheral, meaning that on
	  disconnecting the respective monitor, all views will be moved to another
	  available output. The default role is peripheral.
	- prio <n\> is used to set the priority of an output. If
	  nothing else is set, outputs are added as they request to be added
	  and have a numerical priority of -1. Using prio <n\> it is possible
	  to set priorities for outputs, where <n\> >= 1. The larger <n\> is,
	  the higher the priority is, that is to say, the earlier the output
	  will appear in the list of outputs.
	- rotate <n\> is used to rotate the output by `<n> mod 4 x 90` degrees
	  counter-clockwise.

```
# Don't rotate
output DP-1 rotate 0

# rotate 90 degrees counter-clockwise
output DP-1 rotate 1

# rotate 180 degrees counter-clockwise
output DP-1 rotate 2

# rotate 270 degrees counter-clockwise
output DP-1 rotate 3
```

	*output* and the *screen* family of commands are similar in that they
	both deal with monitors on some level.
	- *output* addresses outputs by their name and is vaguely symmetric
	  to *input*.
	- Any *screen* command deals with the number identifying a
	  monitor within a Cagebreak session either explicitly or
	  implicitly (i.e. the commands containing next and prev).

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

*screen <n\>*
	Change to <n\>-th screen
	See *output* for differences between screen and output.

*show_info*
	Display information about the current setup - In particular, print the identifiers
	of the available inputs and outputs.

*setmode <mode\>*
	Set default mode to <mode\>

*switchvt <n\>*
	Switch to tty <n\>

*time*
	Display time

*vsplit*
	Split current tile vertically

*workspace <n\>*
	Change to <n\>-th workspace

*workspaces <n\>*
	Set number of workspaces to <n\> - <n\> is a single integer larger than 1
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

	<mod\>-<key\>

The supported modifiers are:

*A - Alt*

*C - Control*

*L - Logo*

*S - Shift*

*2 - Mod 2*

*3 - Mod 3*

*5 - Mod 5*

For example to specify the keybinding Control+t, the expression

```
C-t
```

is used.

# SEE ALSO

*cagebreak(1)*
*cagebreak-socket(7)*

# BUGS

See GitHub Issues: <https://github.com/project-repo/cagebreak/issues>

Mail contact: `cagebreak @ project-repo . co`

GPG Fingerprints:

- 0A268C188D7949FEB39FD1462F2AD980247E4918
- 283D10F54201B0C6CCEE2C561DE04E4B056C749D

# LICENSE

Copyright (c) 2020-2024 The Cagebreak authors

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
