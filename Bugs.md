# Bugs

This file is slightly unusual to exist given that the project is currently
developed on github which offers an issue tracker. However, documenting
bugs inside of the repository makes the project more robust, should it ever
be moved to another hosting provider with a different issue tracking approach
or in case of network outages, where the github issue tracker may be unavailable
for developers. It also makes possible the description of bugs which were not
submitted on the issue tracker, should any other forms of contact become
available.

## Issues

### Issue 1

  * Github issue number: N/A
  * Fixed: 1.0.2

This issue is a focussing bug when using anki in release 1.0.1 and earlier
on arch linux systems.

Steps to reproduce:

  * start anki
  * Click "Add" to add cards
  * enter text into the "Tags" field
  * if autocompletion of tags pops up, the keyboard will not enter further
    keystrokes into the text field

### Issue 2

  * Github issue number: N/A
  * Fixed: 1.0.4

This issue fixes wrong consumption of modifier key on some keyboards.

### Issue 3

  * github issue number: N/A
  * Fixed: 1.0.5

This issue is a bug causing crashes in cagebreak under specific circumstances.

Steps to reproduce:

  * start cagebreak
  * vertical split
  * open a terminal on the left pane
  * open firefox on the right pane
  * focus the left pane
  * close the terminal
  * close firefox using the mouse while keeping focus on the left pane
  * this causes a crash with the following error message:

```
(EE) failed to read Wayland events: Broken pipe
```

### Issue 4

  * github issue number: #1
  * Fixed: 1.1.0

This issue is code duplication in `parse.c`.

Github issue text:

```
As of right now, the actions which can be run in the config file and the
actions which can be run as a keybinding are parsed separately in `parse.c`.
This leads to a lot of code duplication. Furthermore, unifying these
functionalities would enable a more versatile configuration. For instance, it
would enable the user to write `hsplit` into the configuration file to split
the output on startup and workspace 2 to set the default workspace to
workspace 2. Therefore, this change would simplify the code base, while at
the same time increasing the feature set.

PS: As a side effect, this would allow quirky statements such as
`bind d bind r hsplit` which would bind the d key to binding the r key to
split the output...
```

### Issue 5

  * github issue number : N/A
  * Fixed: 1.2.1

Cagebreak up to and including release 1.2.0 does not warn of irreproducibility
if a different compiler or compiler version is used. This makes cagebreak
difficult to reproduce.

### Issue 6

  * github issue number : N/A
  * Fixed: 1.2.1

Our fuzzing framework up to and including release 1.2.0 does not limit line
lengths. This can crash the fuzzing framework with a segfault due to running
out of memory.

### Issue 7

  * github issue number: N/A
  * Fixed: 1.3.1

Cagebreak up to and including release 1.3.0 does not ensure that windows
are resized correctly. The download save dialog in firefox
demonstrates this issue, as it may be rendered larger than the current
monitor. As of release 1.3.1 the size of every window is checked on
rendering and adjusted if necessary.

### Issue 8

  * github issue number: N/A
  * Fixed: 1.3.1

Cagebreak up to and including release 1.3.0 does not force windows
to resize to the appropriate size. Calling `vsplit` on inkscape
demonstrates this issue, since it causes adjacent tiles to overlap
(contrary to the idea of a tiling compositor).

### Issue 9

  * github issue number: N/A
  * Fixed: 1.3.2

Cagebreak up to and including release 1.3.1 sometimes does not render
dropdown menus correctly.

Steps to reproduce:

  * Start firefox with wayland environment variables
  * Press `alt` to show dropdown menus
  * Move cursor over different menu items
  * Rendering will not work correctly

### Issue 10

  * github issue number: N/A
  * Fixed: 1.3.2

Cagebreak up to and including release 1.3.1 does not render drag icons
correctly.

Steps to reproduce:

  * Start firefox with wayland environment variables
  * Click on a link and drag it over the screen
  * Let go of the link
  * Rendering will not work correctly

### Issue 11

  * github issue number: N/A
  * Fixed: 1.3.3

Cagebreak up to and including release 1.3.2 does not parse configuration
"on-keypress" but "on-parse". This does not conform to the documentation.

### Issue 12

  * github issue number: N/A
  * Fixed: 1.3.3

Cagebreak up to and including release 1.3.2 does not render firefox correctly,
given certain splits.

Steps to reproduce:

  * Open firefox
  * Split screen with firefox on the right side of the screen
  * Observe firefox freezing and flickering, especially while scrolling

### Issue 13

  * github issue number: N/A
  * Fixed: 1.4.0

Cagebreak up to and including release 1.3.4 does not render xwayland views correctly.

Steps to reproduce:

  * Open anki
  * Add card
  * Edit tags
  * Observe suggestions not directly above the tags field

### Issue 14

  * github issue number: N/A
  * Fixed: 1.4.1

Cagebreak up to and including release 1.4.0 makes the screen flicker, when
invoking wl-copy or wl-paste.

Steps to reproduce:

  * Open terminal
  * Invoke wl-copy
  * Observe screen flickering

### Issue 15

  * github issue number: N/A
  * Fixed: 1.4.1

Cagebreak up to and including release 1.4.0 renders certain dropdown
menus incorrectly.

Steps to reproduce:

  * Open firefox
  * Download any file
  * Select "save file" such that the file location dialog box appears
  * Delete default file name and enter "." into the location bar, such that a long dropdown menu appears
  * Click on dialog box (not on the dropdown menu) to make the menu disappear
  * Observe flickering of the area, where the dropdown menu used to be

### Issue 16

  * github issue number: N/A
  * Fixed: 1.4.2

Cagebreak up to and including release 1.4.1 has a difficult-to-reproduce
use-after-free bug, which can sometimes trigger crashes when popups are closed.

### Issue 17

  * github issue number: N/A
  * Fixed: 1.4.3

Cagebreak up to and including version 1.4.2 might have a use-after-free
on destroying unmapped views. view->workspace was set on view creation.
If the output containing this view was subsequently destroyed, the workspace
was freed, leading to a use-after-free when the view was destroyed.

### Issue 18

  * github issue number: N/A
  * Fixed: 1.4.3

Cagebreak up to and including release 1.4.2 does not handle the position of
xwayland views adequately.

### Issue 19

  * github issue number: N/A
  * Fixed: 1.4.3

Cagebreak up to and including release 1.4.2 does not handle unmanaged views
adequately.

### Issue 20

  * github issue number: N/A
  * Fixed: 1.4.3

Cagebreak up to and including release 1.4.2 does not handle damaging adequately,
when scanning out views.

### Issue 21

  * github issue number: N/A
  * Fixed: 1.4.4

Cagebreak up to and including release 1.4.3 sometimes had null pointer
dereferences on view destroy.

### Issue 22

  * github issue number: #2
  * Fixed: 1.5.0

Cagebreak up to and including release 1.4.4 did not offer any means to disable
outputs. This option is now added to output. See cagebreak-config man page for
details.

Github issue text:

```
Hello, I have two outputs (or heads) eDP-1 and HDMA-A-1, and always want to disable eDP-1, how to do it?

Thank you!
```

### Issue 23

  * github issue number: N/A
  * Fixed: 1.5.0

Cagebreak up to and including release 1.4.4 had a potential use-after-free bug.

### Issue 24

  * github issue number: N/A
  * Fixed: 1.5.0

Cagebreak up to and including release 1.4.4 could have focussed views without tiles.

