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

### Issue 24

  * github issue number: N/A
  * Fixed: 1.6.0

Cagebreak up to and including release 1.5.1 had an error, where the code
incremented a variable and not a pointer. This resulted in a bug in a
surface counting iterator.

### Issue 25

  * github issue number: N/A
  * Fixed: 1.6.0

Cagebreak, beginning with release 1.5.0, when a keybinding containing an output
configuration is removed from the list of active keybindings, the output
configuration contained in this keybinding is destroyed in order to
prevent memory leaks. However, after an output configuration was applied,
it was inserted into the list of active output configurations and if it
was later destroyed, this led to a use-after-free memory corruption.
Starting from release 1.6.0, output configurations are copied before
being inserted into the list of active output configurations and
therefore remain valid even if the original is freed.

### Issue 26

  * github issue number: #5
  * Fixed: 1.7.1

```
Hello!

I am, huh, a mere package maintainer. This is a simple suggestion: use scdoc to generate manpages, instead of pandoc.

The advantages are:

    scdoc is tailored to generate manpages from markdown-like sources;
    scdoc is smaller
    scdoc has less dependencies
```

Cagebreak has always used pandoc to compile man pages. Cagebreak now uses scdoc
instead.

### Issue 27

  * github issue number: #8
  * Fixed: 1.8.1

```
Apologies for opening two tickets instead of one, but I thought that these two suggestions are different enought they merit their own "issue".

Would replicating the behaviour in ratpoison (if no config file is found, load predefined defaults) something you would be interested in? I personally found confusing when I first tried cagebreak that it wouldn't start without a config file, coming from ratpoison. Of course, other people's experience might be different.
```

### Issue 28

  * github issue number: #10
  * Fixed: 1.8.2

```
I'm using v. 1.8.1 and am experiencing a screen refresh lag, particularly on Firefox:

When I type in anything, the text is refreshed and becomes visible only following mouse movement. I don't experience this with the same setup when I use Sway as the compositor.
```

Cagebreak had an error with damage tracking.

### Issue 29

  * github issue number: #19
  * Fixed: 1.9.0

bda65hp:
```
Hi,
Is there a way to easily change de font? "monospace 10" are too small for my eyes. I've changed them in message.c but can you add a configuration option, please?
Same for the display time of these window. They sometimes disappear too fast (mainly time).
May be a config for the background and foreground color of the message window too?

cagebreak is great :)
```

project-repo:
```
No, there is currently no easy way to change the font size, background or display
time of the message.

Adding these ideas as a command is a good idea though.

To summarize, the release pertaining to #19 will presumably introduce a
command for configuring the cagebreak messages. This command will be
able to set the following:

 * [ ] Font size
 * [ ] Font colour
 * [ ] Background colour
 * [ ] Display time of message

Another, related, idea would be to have an interface for sending
arbitrary messages over the socket which are then displayed by
cagebreak. This may also be added to the above features.

We're glad to hear that you enjoy cagebreak! Thank you for helping to
make it even greater.

cheers
project-repo
```

bda65hp:
```
Awesome :)
Displaying time in locale format (or configurable) will be great too ;)
```

project-repo:
```
Yes, once it is possible to send arbitrary messages via cagebreak over
the socket it will be possible to have the date be displayed however you
like!
cheers
project-repo
```

project-repo:
```
We pushed some code to development implementing this in 348925e (configuration) and f12a161 (message command). Sorry for not writing this much sooner. However, at least it will be part of the upcoming release.

cheers
project-repo
```

###   Issue 30

  * github issue number: #11
  * Fixed: 1.9.0

kinleyd:
```
In my first post I forgot to thank you for cagebreak. I've been evaluating compositors for my needs and have been through kiwmi, japokwm, wayfire, sway and cagebreak. So far I like cagebreak most of all.

Back to my question:

You currently support 'movetonextscreen' but there is no equivalent movetoprevscreen.
For multiple 3 to 4 screen configurations the second command would be useful.

Also, is it not possible to move to a screen directly, like 'movetoscreen 3'?

The same thing with prevscreen and nextscreen: something like 'focusscreen 3', if it were possible, would be useful.
```

project-repo:
```
Thank you! It's great to hear that cagebreak seems to suit your needs.

About your question, yes, that absolutely makes sense. The reason this
wasn't implemented earlier is that no-one has needed it up to now, but
seeing as there now appears to be a use-case, we can definitely
implement these features. In future releases, cagebreak will then most
likely support movetoprevscreen and movetoscreen <n> (as these are
distinct functionalities).

The only issue I see at this point is that the numbering of screens is
not directly canonical. To resolve this, I suggest that per default,
screens are numbered according to the order in which they were added
along with an additional configuration option to specify numbering (this
would most likely be an additional option for the already existent
output command). Does that seem fine to you?
cheers,
project-repo
```

kinleyd:
```
Yes, that would be perfect. Thank you!
```

kinleyd:
```
May I suggest we also have 'screen <n>' like we have 'workspace <n>'.
```

project-repo:
```
Thank you for this suggestion. This will be implemented in the release
where the rest of this issue is resolved (probably the next one).

To summarize, presumably the release pertaining to #11 will include:

  * [ ] movetoprevscreen as a command symmetric to movetonextscreen
  * [ ]  movetoscreen <n> as a command similar to movetoworkspace
  * [ ]  screen <n> as a command similar to workspace <n>
  * [ ]  additional output functionality for monitor numbering (of
         course backwards-compatible, such that valid configs remain
         valid)

If anything changes with regard to this draft, we will post it here so
you can let us know what you think.

cheers,
project-repo
```

kinleyd:
```
Super - I greatly look forward to their addition, and thank you for your responsiveness!
```

project-repo:
```
Everything mentioned on our last comment has now been implemented on the
development branch. (More specifically in 851a2f8,
though documentation was added later.)

The simple commands work as proposed in our previous comment.
The output command has a new feature called prio now, which you can call as follows:

output <name of the output> prio <n>

All outputs, regardless of configuration are added once they request to be added
and have a default priority of -1. Using prio, you can set priorities
greater than or equal to 1 for specific outputs and hence order your outputs
as you wish. Note that cagebreak -s might be useful while fiddling around
with this.

If you like, you can check it out on development and compile it for yourself.
If you're up to it we would be glad to know what you think about it,
especially when output prio is applied to your multi-screen setup and
regarding the readability of the cagebreak-config man page description of
output prio.

Please note that this is not a release yet and the development branch
might undergo changes.

cheers
project-repo
```

kinleyd:
```
I took the development branch for a swirl, and it looks good. Thank you!

Everything works as you had set out: screen, movetoscreen, movetoprevscreen and output screen prio all work as expected. The default screen position now reflects the prio settings and that's great.

I took a look at the updates to the man page as well and it looks quite clear.

So hey, thank you for the great work! I look forward to the incorporation of the new features in the master branch when it is ready.

Happy holidays and a happy new year!
```

### Issue 31

  * github issue number: #25
  * Fixed: 1.9.0

```
The configuration man page for the input send events setting is missing the events word:

diff --git man/cagebreak-config.5.md man/cagebreak-config.5.md
index 4eea4c5..c618314 100644
--- man/cagebreak-config.5.md
+++ man/cagebreak-config.5.md
@@ -123,7 +123,7 @@ by prepending a line with the # symbol.
    *dwt enabled|disabled*
         Enables or disables disable-while-typing for the specified input device.

          -       *enabled|disabled|disabled_on_external_mouse*
          - +     *events enabled|disabled|disabled_on_external_mouse*
                            Enables or disables send_events for specified input device. Disabling
                            send_events disables the input device.
Slightly off topic: I was hoping to use this setting to hide the mouse cursor, but it doesn't work. My embedded system must expose a pointer device even though none is connected, altough I can only infer that much because the system is very bare bones and I can't check. Is there a way to do that?
```

The word was omitted before 1.9.0. Mouse hiding was relegated to a new issue (#26).

### Issue 32

  * github issue number: #24
  * Fixed: 1.9.0

```
The wlr/xwayland.h include in output.c:32 is missing an #if CG_HAS_XWAYLAND guard, which breaks building without xwayland.
```

Cagebreak did not build without xwayland. This bug has been fixed and building
without xwayland has been added to the release checklist.

### Issue 33

  * github issue number: #23
  * Fixed: 1.9.0

```
Similar to djpohly/dwl#177. Xwayland works fine elsewhere: cage, hikari, kwinft, labwc, phoc, river, sway, wayfire.
```
$ cagebreak
[...]
00:00:00.085 [xwayland/server.c:92] Starting Xwayland on :0
00:00:00.116 [render/swapchain.c:105] Allocating new swapchain buffer
00:00:00.118 [render/allocator/gbm.c:142] Allocated 1280x720 GBM buffer (format 0x34325258, modifier 0x100000000000004)
00:00:00.118 [render/gles2/renderer.c:143] Created GL FBO for buffer 1280x720
00:00:00.160 [types/wlr_surface.c:748] New wlr_surface 0x880d81f80 (res 0x86f22d380)
00:00:00.165 [xwayland/server.c:243] waitpid for Xwayland fork failed: No child processes
(EE) failed to read Wayland events: Broken pipe
xterm: Xt error: Can't open display: :0

$ cc --version
FreeBSD clang version 11.0.1 (git@github.com:llvm/llvm-project.git llvmorg-11.0.1-0-g43ff75f2c3fe)
Target: x86_64-unknown-freebsd13.0
Thread model: posix
InstalledDir: /usr/bin

$ pkg info -x cagebreak wayland wlroots
cagebreak-1.8.3
wayland-1.20.0
wayland-protocols-1.25
xwayland-devel-21.0.99.1.171
wlroots-0.15.1
```
```
