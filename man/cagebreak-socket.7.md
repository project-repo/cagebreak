cagebreak-socket(7) "Version 2.2.0" "Cagebreak Manual"

# NAME

*cagebreak-socket* — Cagebreak socket

# SYNOPSIS

*ipc-socket-capable-tool \$CAGEBREAK_SOCKET*

# DESCRIPTION

The cagebreak socket is an ipc socket.

The socket is enabled if cagebreak is invoked with the `-e` flag.

The socket accepts cagebreak commands as input (see *cagebreak-config(5)* for more information).

Events are provided as output as specified in this man page.

## EVENTS

Events have a general structure as follows:

```
"cg-ipc"json object depending on the eventNULL
```

Here is an example of how this works using *only* as a command sent over the socket.

```
only
cg-ipc{"event_name":"fullscreen","tile_id":2,"workspace":1,"output":"eDP-1"}
```

This documentation describes the trigger for the events, the keys and the data
type of the values of each event.

*background*
	- Trigger: *background* command
	- JSON
		- event_name: "background"
		- old_bg: list of three floating point numbers denoting the old background in rgb
		- new_bg: list of three floating point numbers denoting the new background in rgb

```
background 0 1.0 0
cg-ipc{"event_name":"background","old_bg":[0.000000,1.000000,1.000000],"new_bg":[0.000000,1.000000,0.000000]}
```

*close*
	- Trigger: *close* command
	- JSON
		- event_name: "close"
		- view_id: view id as an integer
		- tile_id: tile id as an integer
		- view_pid: pid of the process
		- workspace: workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
close
cg-ipc{"event_name":"close","view_id":47,"view_pid":30456,"tile_id":47,"workspace":1,"output":"eDP-1","output_id":1}
```

*configure_input*
	- Trigger: *input* command
	- JSON
		- event_name: "configure_input"
		- input: the input as a string, as per cagebreak-config(5)

```
input * accel_profile flat
cg-ipc{"event_name":"configure_input","input":"*"}
```

*configure_message*
	- Trigger: *configure_message* command
	- JSON
		- event_name: "configure_message"

```
configure_message fg_color 1.0 1.0 0 0
cg-ipc{"event_name":"configure_message"}
```

*configure_output*
	- Trigger: *output* command
	- JSON
		- event_name: "configure_output"
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
output eDP-1 rotate 0
cg-ipc{"event_name":"configure_output","output":"eDP-1","output_id":1}
```

*cursor_switch_tile*
	- Trigger: Cursor crosses the border between tiles
	- JSON
		- event_name: "cursor_switch_tile"
		- old_output: name of the old output as a string
		- old_output_id: old output id as an integer
		- old_tile: number of the old tile as an integer
		- new_output: name of the new output as a string
		- new_output_id: new output id as an integer
		- new_tile: number of the new tile as an integer

```
# Cursor switches tile
cg-ipc{"event_name":"cursor_switch_tile","old_output":"eDP-1","old_output_id":1,"old_tile":2,"new_output":"eDP-1","new_output_id":1,"new_tile":3}
```

*custom_event*
	- Trigger: Cagebreak receives the *custom_event* command, either in the config file or via the IPC socket (see *cagebreak-config(5)*).
	- JSON
		- event_name: "custom_event"
		- message: The message passed to the *custom_event* command

```
custom_event Hello World!
cg-ipc{"event_name":"custom_event","message":"Hello World!"}
```

*cycle_outputs*
	- Trigger: *nextscreen* and *prevscreen* commands
	- JSON
		- event_name: "cycle_outputs"
		- old_output: old output name as string
		- old_output_id: old output id as an integer
		- new_output: new output name as string
		- new_output_id: new output id as an integer
		- reverse: "0" if *nextscreen* or "1" if *prevscreen*

```
nextscreen
cg-ipc{"event_name":"cycle_outputs","old_output":"eDP-1","old_output_id":1,"new_output":"HDMI-A-1","new_output_id":2,"reverse":0}
```

*cycle_views*
	- Trigger: *next* and *prev* commands
	- JSON
		- event_name: "cycle_views"
		- old_view_id: old view id as an integer
		- old_view_pid: pid of old view
		- new_view_id: new view id as an interger
		- new_view_pid: pid of new view
		- tile_id: tile id as an integer
		- workspace: workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
next
cg-ipc{"event_name":"cycle_views","old_view_id":11,"old_view_pid":32223,"new_view_id":4,"old_view_pid";53221,"tile_id":13,"workspace":1,"output":"eDP-1","output_id":1}
```

*definekey*
	- Trigger: *definekey* command
	- JSON
		- event_name: "definekey"
		- modifiers: number denoting the modifier as described below
			- 0: no modifier
			- 1: shift
			- 2: alt
			- 3: ctrl
			- 4: logo key
			- 5: modifier 2
			- 6: modifier 3
			- 7: modifier 5
		- key: key as a number
		- command: command as a string - CAVEAT: This is an internal representation of
		  commands which is not in one-to-one correspondance with the commands available in the config file. The differences are as follows:
			- "cycle_tiles": represents any *exchange* command (e.g. *exchangeright*)
			- "cycle_views": represents both the *next* and the *prev* command
			- "cycle_outputs": represents *nextscreen*, *movetoprevscreen* and *prevscreen* commands
			- "resize_tile_vertical": represents *resizedown* and *resizeup* commands
			- "resize_tile_horizontal": represents *resizeleft* and *resizeright* commands

```
definemode foo
cg-ipc{"event_name":"definemode","mode":"foo"}
definekey foo e only
cg-ipc{"event_name":"definekey","modifiers":0,"key":101,"command":"only"}
```

*definemode*
	- Trigger: *definemode* command
	- JSON
		- event_name: "definemode"
		- mode: name of mode as string

```
definemode foo
cg-ipc{"event_name":"definemode","mode":"foo"}
```

*destroy_output*
	- Trigger: removal of an output
	- JSON
		- event_name: "destroy_output"
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
# remove output from the device
cg-ipc{"event_name":"destroy_output","output":"HDMI-A-1","output_id":2}
```

*dump*
	- Trigger: *dump* command
	- JSON
		- event_name: "dump"
		- nws: number of workspaces as an integer
		- bg_color: list of three floating point numbers denoting the new background in rgb
		- views_curr_id: id of the currently focussed view as an integer
		- tiles_curr_id: id of the currently focussed tile as in integer
		- curr_output: current output as a string
		- default_mode: name of the default mode as a string
		- modes: list of names of modes as strings
		- outputs: object of objects for each output
			- output name as string
				- priority: priority as per *output* prio <n> in *cagebreak-config(5)* or default
				- coords: object of x and y coordinates of output
				- size: object of width and height as integers
				- refresh_rate: refresh rate as float
				- curr_workspace: current workspace as an integer
				- workspaces: list of objects for each workspace
					- views: list of objects for each view
						- id: view id as an integer
						- pid: pid of the process which opened the view as an integer
						- coords: object of x and y coordinates
						- type: ["xdg"|"xwayland"]
					- tiles: list of objects for all tiles
						- id: tile id as an integer
						- coords: object of x and y coordinates
						- size: object of width and height
						- view: view id as an integer
		- keyboards: object of objects for each keyboard group
			- keyboard name as a string
				- commands_enabled: 0 if keybindings are disabled for the keyboard, 1 otherwise
				- repeat_delay: repeat delay in milliseconds as an integer
				- repeat_rate: repeat rate in 1/sec as an integer
		- input_devices: object of objects for each keyboard
			- identifier for a keyboard as a string
				- is_virtual: 1 if virtual, 0 otherwise
				- type: [keyboard|pointer|switch]
		- cursor_coords: object of x and y coordinates

```
dump
cg-ipc{"event_name":"dump","nws":1,
"bg_color":[0.000000,0.000000,0.000000],
"views_curr_id":80,
"tiles_curr_id":8,
"curr_output":"eDP-1",
"default_mode":"top",
"modes":["top","root","resize"],
"outputs": {"eDP-1": {
"priority": -1,
"coords": {"x":0,"y":0},
"size": {"width":2560,"height":1440},
"refresh_rate": 60.012000,
"curr_workspace": 0,
"workspaces": [{"views": [{
"id": 16,
"pid": 2505,
"coords": {"x":0,"y":0},
"type": "xwayland"

},{
"id": 72,
"pid": 11243,
"coords": {"x":0,"y":0},
"type": "xdg"

},{
"id": 56,
"pid": 6700,
"coords": {"x":1280,"y":0},
"type": "xdg"

}],"tiles": [{
"id": 6,
"coords": {"x":0,"y":0},
"size": {"width":1280,"height":1440},
"view": 78

},{
"id": 7,
"coords": {"x":1280,"y":0},
"size": {"width":1280,"height":1440},
"view": 42

}]}]
}}
,"keyboards": {"0:1:Power_Button": {
"commands_enabled": 1,
"repeat_delay": 600,
"repeat_rate": 25
}}
,"input_devices": {"6058:20564:ThinkPad_Extra_Buttons": {
"is_virtual": 0,
"type": "switch",
},"6058:20564:ThinkPad_Extra_Buttons": {
"is_virtual": 0,
"type": "keyboard",
},"2:10:TPPS/2_Elan_TrackPoint": {
"is_virtual": 0,
"type": "pointer",
},"1:1:AT_Translated_Set_2_keyboard": {
"is_virtual": 0,
"type": "keyboard",
},"0:0:sof-hda-dsp_Headphone": {
"is_virtual": 0,
"type": "keyboard",
},"1739:52619:SYNA8004:00_06CB:CD8B_Touchpad": {
"is_virtual": 0,
"type": "pointer",
},"1739:52619:SYNA8004:00_06CB:CD8B_Mouse": {
"is_virtual": 0,
"type": "pointer",
},"0:3:Sleep_Button": {
"is_virtual": 0,
"type": "keyboard",
},"0:5:Lid_Switch": {
"is_virtual": 0,
"type": "switch",
},"0:6:Video_Bus": {
"is_virtual": 0,
"type": "keyboard",
},"0:1:Power_Button": {
"is_virtual": 0,
"type": "keyboard",
}}
,"cursor_coords":{"x":972.821761,"y":670.836215}
}
```

*focus_tile*
	- Trigger: *focus* command
	- JSON
		- event_name: "focus_tile"
		- old_tile_id: old tile id as an integer
		- new_tile_id: new tile id as an integer
		- workspace: workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
focus
cg-ipc{"event_name":"focus_tile","old_tile_id":14,"new_tile_id":13,"workspace":1,"output":"eDP-1","output_id":1}
```

*fullscreen*
	- Trigger: *only* command
	- JSON
		- event_name: "fullscreen"
		- tile_id: tile id as an integer
		- workspace: workspace number as an integer
		- output: output as a string
		- output_id: id of the output as an integer

```
only
cg-ipc{"event_name":"fullscreen","tile_id":3,"workspace":1,"output":"eDP-1","output_id":1}
```

*move_view_to_cycle_output*
	- Trigger: *movetonextscreen* and similar commands
	- JSON
		- event_name: "move_view_to_cycle_output"
		- view_id: view id as an integer
		- view_pid: pid of the process
		- old_output: name of the old output as a string
		- old_output_id: old output id as an integer
		- new_output: name of the new output as a string
		- old_output_id: old output id as an integer

```
movetonextscreen
cg-ipc{"event_name":"cycle_outputs","old_output":"eDP-1","new_output":"HDMI-A-1","reverse":0}
cg-ipc{"event_name":"move_view_to_cycle_output","view_id":11,"view_pid":43123,"old_output":"eDP-1","old_output_id":1,"new_output":"HDMI-A-1","new_output_id":2}
```

*move_view_to_output*
	- Trigger: *movetoscreen* command
	- JSON
		- event_name: "move_view_to_output"
		- view_id: view id as an integer
		- old_output: old output name as string
		- new_output: new output name as string

```
movetoscreen 2
cg-ipc{"event_name":"switch_output","old_output":"eDP-1","new_output":"HDMI-A-1"}
cg-ipc{"event_name":"move_view_to_output","view_id":78,"old_output":"eDP-1","new_output":"HDMI-A-1"}
```

*move_view_to_ws*
	- Trigger: *movetoworkspace* command
	- JSON
		- event_name: "move_view_to_ws"
		- view_id: view id as an integer
		- old_workspace: old workspace number as an integer
		- new_workspace: new workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer
		- view_pid: pid of the process

```
movetoworkspace 1
cg-ipc{"event_name":"switch_ws","old_workspace":1,"new_workspace":1,"output":"eDP-1"}
cg-ipc{"event_name":"move_view_to_ws","view_id":43,"old_workspace":0,"new_workspace":0,"output":"eDP-1","output_id":1,"view_pid":64908}
```

*new_output*
	- Trigger: a new output is physically attached
	- JSON
		- event_name: "new_output"
		- output: new output name as a string
		- output_id: id of the new output as an integer
		- priority: priority as per *output* prio <n> in *cagebreak-config(5)* or default

```
# a new output is attached
cg-ipc{"event_name":"new_output","output":"HDMI-A-1","output_id":2,"priority":-1}
```

*resize_tile*
	- Trigger: the *resize* family of commands
	- JSON
		- event_name: "resize_tile"
		- tile_id: tile id as an integer
		- old_dims: list of coordinates [x coordinate of lower left corner, y coordinate of lower left corner, x coordinate of upper right corner, y coordinate of upper right corner]
		- new_dims: list of coordinate [x coordinate of lower left corner, y coordinate of lower left corner, x coordinate of upper right corner, y coordinate of upper right corner]
		- workspace: workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
resizeleft
cg-ipc{"event_name":"resize_tile","tile_id":14,"old_dims":"[1280,0,1440,1280]","new_dims":"[1270,0,1440,1290]","workspace":1,"output":"eDP-1","output_id":1}
cg-ipc{"event_name":"resize_tile","tile_id":13,"old_dims":"[0,0,1440,1280]","new_dims":"[0,0,1440,1270]","workspace":1,"output":"eDP-1","output_id":1}
```

*set_nws*
	- Trigger: *workspaces* command
	- JSON
		- event_name: "set_nws"
		- old_nws: old number of workspaces as an integer
		- new_nws: new number of workspaces as an integer

```
workspaces 2
cg-ipc{"event_name":"set_nws","old_nws":1,"new_nws":2}
```

*split*
	- Trigger: *split* command
	- JSON
		- event_name: "split"
		- tile_id: old tile id as an integer
		- new_tile_id: new tile id as an integer
		- workspace: workspace number as an integer
		- output: output as a string
		- output_id: id of the output as an integer
		- vertical: 0 if horizontal split, 1 if not

```
hsplit
cg-ipc{"event_name":"split","tile_id":11,"new_tile_id":12,"workspace":1,"output":"eDP-1","output_id":1,"vertical":0}
```

*swap_tile*
	- Trigger: the *exchange* family of commands
	- JSON
		- event_name: "swap_tile"
		- tile_id: previous tile id as an integer
		- tile_pid: pid of previous tile
		- swap_tile_id: swap tile id as an integer
		- swap_tile_pid: pid of swap tile
		- workspace: workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
exchangeright
cg-ipc{"event_name":"swap_tile","tile_id":1,"tile_pid":53478,"swap_tile_id":3,"swap_tile_pid":98234,"workspace":1,"output":"eDP-1"}
```

*switch_default_mode*
	- Trigger: *setmode* command
	- JSON
		- event_name: "switch_default_mode"
		- old_mode: old mode name
		- mode: new mode name

```
setmode top
cg-ipc{"event_name":"switch_default_mode","old_mode":"top","mode":"root"}
```

switch_output
	- Trigger. *screen* command
	- JSON
		- event_name: "switch_output"
		- old_output: name of the old output as a string
		- old_output_id: old output id as an integer
		- new_output: name of the new output as a string
		- new_output_id: new output id as an integer

```
screen 2
cg-ipc{"event_name":"switch_output","old_output":"eDP-1","old_output_id":1,"new_output":"HDMI-A-1","new_output_id":2}
```

switch_ws
	- Trigger: *workspace* command
	- JSON
		- event_name: "switch_ws"
		- old_workspace: old workspace number as an integer
		- new_workspace: new workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer

```
workspace 2
cg-ipc{"event_name":"switch_ws","old_workspace":1,"new_workspace":2,"output":"eDP-1","output_id":1}
```

*view_map*
	- Trigger: view is opened by a process
	- JSON
		- event_name: "view_map"
		- view_id: view id as an integer
		- tile_id: tile id as an integer
		- workspace: workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer
		- view_pid: pid of the process

```
# process opens a view
cg-ipc{"event_name":"view_map","view_id":28,"tile_id":14,"workspace":1,"output":"eDP-1","output_id":1,"view_pid":39827}
```

*view_unmap*
	- Trigger: view is closed by a process
	- JSON
		- event_name: "view_unmap"
		- view_id: view id as an integer
		- tile_id: tile id as an integer
		- workspace: workspace number as an integer
		- output: name of the output as a string
		- output_id: id of the output as an integer
		- view_pid: pid of the process

```
# view is closed by the process
cg-ipc{"event_name":"view_unmap","view_id":24,"tile_id":13,"workspace":1,"output":"eDP-1","output_id":1,"view_pid":39544}
```

## SECURITY

The socket has to be explicitly enabled using the `-e` flag.

The socket is restricted to the user for reading, writing and execution (700).

All user software can execute arbitrary code while the cagebreak socket is running.

## EXAMPLES

*nc -U $CAGEBREAK_SOCKET*

*ncat -U $CAGEBREAK_SOCKET*

# SEE ALSO

*cagebreak(1)*
*cagebreak-config(5)*

# BUGS

See GitHub Issues: <https://github.com/project-repo/cagebreak/issues>

Mail contact: `cagebreak @ project-repo . co`

GPG Fingerprints:

- B15B92642760E11FE002DE168708D42451A94AB5
- F8DD9F8DD12B85A28F5827C4678E34D2E753AA3C
- 3ACEA46CCECD59E4C8222F791CBEB493681E8693
- 0A268C188D7949FEB39FD1462F2AD980247E4918

# LICENSE

Copyright (c) 2022 - 2023 The Cagebreak authors

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
