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
`bind dbind r hsplit` which would bind the d key to binding the r key to
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

Our fuzzing framework up to and including release 1.2.0 does not the limit line
lengths. This can crash the fuzzing framework with a segfault due to running
out of memory.
