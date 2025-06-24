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

## Issue 34

  * github issue number: N/A
  * Fixed: 1.9.0

Cagebreak crashed when a touch device was removed due to a call to
`wl_list_remove` on a pointer which was never initialized.

## Issue 35

  * github issue number: N/A
  * Fixed: 1.9.1

Cagebreak 1.9.0 broke the nextscreen and prevscreen functionality.
This issue was not present in earlier releases.

## Issue 36

  * github issue number: N/A
  * Fixed: 1.9.1

Cagebreak 1.9.0 broke the configure_message functionality.
This issue was not present in earlier releases.

## Issue 37

  * github issue number: N/A
  * Fixed: 1.9.1

Cagebreak 1.9.0 broke the screen/movetoscreen functionality.
This issue was not present in earlier releases.

## Issue 38

  * github issue number: #31
  * Fixed: 2.0.0

Cagebreak terminology was unclear up to and including 1.9.1.

The following are extracts from the github issue discussion:

kinleyd:
```
This isn't an issue per se, but concerns cagebreak terminology.
I won't try to cover the entire range of terms used but stick
to the ones that concern me at the moment:

OK, so I get the use of the terms 'output', 'workspace' and 'tile'.

So my question: In cagebreak, what exactly is a 'screen'? At the
moment, the use of nextscreen, prevscreen, movetoscreen,
movetonextscreen, movetoprevscreen all seem to suggest that
'screen' is the same as 'output'. If so, shouldn't all the
aforementioned commands have screen replaced with output?
eg. nextoutput, prevoutput, etc.
```

project-repo
```
You make an interesting point.

We have considered this and the tricky bit is that both output and *screen are
defined cagebreak commands. It would be difficult to merge these two
commands in a reasonable way (even with the freedom of breaking changes).

Additionally, there is a sort of informal symmetry between output and input
we would like to maintain.

Our current conclusion is that we will leave the commands unmodified.
The now ex-post defined distinction between output and *screen is that
the *screen stuff works with the natural number uniquely identifying the
output within a given cagebreak session (but not across sessions, "nextscreen"
and "prevscreen" do so internally by in-/decrementing this number), whereas
output always deals with an output name as does input with an input name.

Please feel free to point out any other inconsistencies in cagebreak or better
solutions than outlined above in this issue.

You can post your suggestions here.

We will close this issue once 2.0.0 is released where the points mentioned above
will be outlined in the documentation. Please leave it open in
the mean time. Afterwards, you may open a new issue.

cheers
project-repo
```

kinleyd:
```
> Our current conclusion is that we will leave the commands unmodified.
> The now ex-post defined distinction between output and *screen is
> that the *screen stuff works with the natural number uniquely
> identifying the output within a given cagebreak session (but not
> across sessions, "nextscreen" and "prevscreen" do so internally
> by in-/decrementing this number), whereas output always deals
> with an output name as does input with an input name.

That makes perfect sense. Additionally, I also realize that there
is the virtual display for which the concept of 'screen' as
distinct from 'output' is necessary as a screen could then be
a fraction of an output, or combine multiple outputs.

> We will close this issue once 2.0.0 is released where the points
> mentioned above will be outlined in the documentation. Please
> leave it open in the mean time. Afterwards, you may open a new issue.

Hey, I look forward to version 2.0.0!
```

project-repo:
```
We have now modified the cagebreak-config man page on the development
branch to clarify the relationship of output and the screen family of
commands.

Please let us know if anything can be improved in that regard.

cheers
project-repo
```

## Issue 39

  * github issue number: #30
  * Fixed: 2.0.0

Cagebreak 1.9.1 had an issue where clicks did not evoke
any response under some very specific circumstances.

This was remediated by switching Cagebreak to `wlr_scene`.

There was extensive discussion inside the github issue
but this is omitted here due to irrelevance.

## Issue 40

  * github issue number: #20
  * Fixed: 2.0.0

nor-0 proposed a more intuitive way to switch focus. This
is a breaking change and as such is introduced on a major
release.

Details are described in the github issue discussion, which
is reproduced here:

nor-0:
```
Hello,

I think switching focus is a bit confusing in some situations.
When focus is switched in a direction, it can end up diagonally
instead of the frame next to the previously focused one.
The minimal example for this is with four frames, but it
also applies in layouts with more than four of them.

Let´s say I press the keybindings for these actions:
`vsplit`, `hsplit`, `focusright`, `hsplit`.
This leaves me with a layout of four simple quarters:

> +-----+-----+
> |  1  |  2  |
> +-----+-----+
> |  3  |  4  |
> +-----+-----+

Now, using `focuslefton` frame 2 brings me to frame 3 instead
of one. Likewise, `focusright` on frame 3 brings me to frame
2 instead of frame 4.

As soon as I resize a width, switching works as expected and
desired. The oddness however reappears whenever the vertical
frame edges are realigned.

The same trouble happens when creating the layout with the
keybindings for `hsplit`, `vsplit`, `focusdown`, `vsplit`.
Now, focusing up from frame 3 brings me two frame 2 instead
of 1, and same with the symmetrical opposite again
(2 downwards is 3 instead of 4). Now resizing heights is what
helps until the horizontal edges are realigned.

As said before, similar things occur with more complex layouts.
I use those much less frequently of course, but intuitive
navigation is even more important then. In these two minimal
examples, reaching the frames 1 and 4 is also often more
tedious then necessary.

I understand this is due to the way the frames were created
and makes some sense in this regard. However, unless there
is some reason to keep this behavior (which disappears by
some resize options anyways), I´d suggest to change it to
be more intuitive.

I imagine an approach for this could be to determine the
frames center point and then looking for an adjacent frame
in horizontal (vertical) direction from there when
`focusright` or `focusleft` (`focusdown` or `focusup`) is used.

(Everything said about `focus[direction]` also applies to
`exchange[direction]`.)
```

project-repo:
```
Thanks for the suggestion! Yes, I agree that the current implementation
is not a very satisfying solution... The reason it is handled like this
is simply because it was easiest to implement. Fortunately however,
changing this should not require too much coding since the
determination of the tile to jump to is handled globally by a single
function for each direction.

I like your idea with considering the center of the frame and then
looking for the one adjacent to it which is at the height of the center,
since such a frame is always guaranteed to exist (unless we are at the
edge of the screen in which case there is nothing to jump to).
Of course, this would have to involve some kind of tie-breaking when
two adjacent frames share a border at the height of the center (which is
bound to occur often if you never resize the tiles). I'm currently
leaning towards simply taking the top/left one in this case (or some
similar scheme) but if anyone else has thoughts on this, we'd be glad to
hear!

Some notes:

  * This would be a breaking change, so the version number would jump to 2.0.0.
  * Also, due to limited developer capacity at the moment and
    the ongoing porting of cagebreak to the wlr_scene API it may be a
    while until we are able to implement this, so bear with us!

cheers
project-repo
```

project-repo:
```
We pushed some code to development implementing this in `ed5cca9`.
Sorry for not writing this much sooner.

To anyone who reads this: This feature will remain on the development
branch for some time, partially due to some unresolved xwayland and
wlr_scene bugs and partially to allow breaking changes. So if anyone
would like to see something changed, please feel free to let us know!

cheers
project-repo
```

## Issue 41

  * github issue number: #12
  * Fixed: 2.0.0

kinleyd raised the issue of changing the focussed screen
on mouse hover. The Cagebreak philosophy is completely
keyboard-oriented and this feature request was denied.
However, this line of inquiry led to the idea of implementing
an ipc socket which would report key events and an information
dump functionality which would enable almost arbitrary scripting.

Related Material:
- [cagebreak-config man page](man/cagebreak-config.md)
- [cagebreak-socket man page](man/cagebreak-socket.md)
- [scripting examples](https://github.com/project-repo/cagebreak/tree/development/example_scripts)

The github issue discussion is partially reproduced below:

kinleyd:
```
While fully recognizing - and appreciating - Cagebreak's keyboard orientation, would it be possible to accommodate support for screen change on mouse over? This would help avoid a series of key presses on multi-screen setups and improve overall ergonomics.

Thanks.
```

project-repo:
```
As you correctly recognised, cagebreak is designed to be completely
keyboard-oriented. However, I understand that for the multi-monitor
setup you describe it may be useful to have some kind of
mouse-compositor interaction. To accommodate this, we are considering
implementing a command which allows to obtain the current cagebreak
state from the ipc socket, including the current cursor position.
That way, by implementing a script which listens for cursor events
and acts accordingly, it should be possible to implement the behaviour
you describe externally (provided #11 is implemented as intended).
What do you think?

If you have any other ideas, don't hesitate to let us know!

cheers
project-repo
```

project-repo
```
There has been much talk about #12 and how it would improve all sorts of
use cases, once implemented. While much remains to be determined, we would
like to give an update.

First, #12 is secondary in priority to #11, which is presumably easier
to implement and much clearer in its scope.

Once implemented, cagebreak will communicate the state of at least the
following internal properties:

  * [ ] Current cursor position (allowing setting of the current screen
        as per navigating screens question and suggestion #11
  * [ ] IDs for graphical programs along with position on screen
  * [ ] IDs for tiles on particular screens
  * [ ] Currently focussed tile, screen, etc.
  * [ ] Events such as "new window", "screen split", etc.

In which format this information will be provided is yet to be
determined, but we are considering at least the following:

  * full blob of data upon request via ipc socket
  * partial information on events

To improve user experience, the release of #12 will include examples of
how to use the new API over the socket for real world use cases,
analogous to the current example config.

  * [ ] Example script "Current screen follows cursor on click"
  * [ ] Example script "Setup multi screen environment with graphical programs on
        specific tiles"

Note that this is just the current state of planning,
the final properties of cagebreak are specified in the man pages upon
release and cagebreak is provided as is, as specified in the LICENSE.

cheers
project-repo
```

project-repo:
```
Some but not all features mentioned in our previous comment have now been implemented on
the development branch.

Information on current events is provided through the ipc socket
now. The types of events which are reported are still subject to change and the
feature is not yet documented but you may check it out nonetheless. Let
us know if there is any additional information you would like to obtain
for the different events.

Current format displayed on the socket is as follows:

'cg-ipc<4 bytes denoting message length><keyword denoting the type of the event>(<property1>:<value1>,<property2>:<value2>,...,<propertyn>:<valuen>)'

Consider it a proof of concept.

A full dump of the current cagebreak state is not yet possible.

Happy new year to you too!
cheers
project-repo
```

project-repo:
```
We pushed some code to development implementing the dump
functionality. This should contain the information you require since it
gives you the view currently focussed by the pointer from which you can
deduce the output the cursor is currently hovering over by looking up
the output on which the view is placed. Note that this only works when
the cursor is actually hovering over a view (i.e. not when it is on an
empty tile). Alternatively, you could read the cursor coordinates and
compare these with the coordinates of the different outputs, though atm
the coordinates of the outputs are not dumped (we aim to add this later).

To anyone who reads this: This feature will remain on the development
branch for some time, partially due to some unresolved xwayland and
wlr_scene bugs and partially to allow breaking changes
(removing/changing data outputted by dump and other information sent by
IPC). So if anyone would like to see something changed, please feel free
to let us know (either by posting here or opening a new issue)!

cheers
project-repo
```

## Issue 42

  * github issue number: N/A
  * Fixed: 2.0.0

When Cagebreak exited due to an error on startup, it was possible for
memory leaks to occur since `server.modes` was not freed correctly. The
same applies to the variable `name` in `input_manager.c`.

This bug was found using the scan-build utility.

## Issue 43

  * github issue number: N/A
  * Fixed: 2.0.0

In `keybinding_move_view_to_cycle_output`, it was previously possible for a
null pointer dereference to occur, when no view to be moved was focussed.

This bug was found using the scan-build utility.

## Issue 44

  * github issue number: N/A
  * Fixed: 2.0.0

`cg_input_reset_libinput_device` was removed as it was unused code.

This bug was found using the scan-build utility.

## Issue 45

  * github issue number: N/A
  * Fixed: 2.0.0

Various memory leaks in `parse_message_config`, `full_screen_workspace`
and `server_show_info` were fixed.

This bug was found using the scan-build utility.

## Issue 46

  * github issue number: #33
  * Fixed: 2.0.0

Cagebrak did not state in its FAQ how to use letters not found
on the current keyboards alphabet.

## Issue 47

  * github issue number: #32
  * Fixed: 2.0.0

Cagebreak did not state in its FAQ how programs can be launched.

## Issue 48

  * github issue number: #16
  * Fixed: 2.0.0

Cagebreak cursor sizes varied under some environmental circumstances.
Investigations showed that the cursor is set by the respective application
but environment variables are usually respected. This is now documented in
the FAQ.

## Issue 49

  * github issue number: #3
  * Fixed: 2.0.0

Cagebreak used to have no way to disable the interpretation of keybindings
for a specific keyboard, which was cumbersome for some applications. This
feature is now available

## Issue 50

  * github issue number: #26
  * Fixed: 2.0.0

Cagebreak used to have no way to hide the cursor. This is now possible.

## Issue 51

  * github issue number: #7
  * Fixed: 2.0.0

Cagebreak used to not change the cursor while waiting for a key. The cursor
is now a square in this state.

## Issue 52

  * github issue number: N/A
  * Fixed: 2.0.0

Cagebreak did not correctly apply some simple commands if they were simply
written in the configuration file.

Consider the example config file:

```
quit
```

Under these circumstances Cagebreak did not quit right after startup.

## Issue 53

  * github issue number: #35
  * Fixed: 2.0.0

Cagebreak did not build with wlroots 0.16.1 and this was adjusted.

## Issue 54

  * github issue numer: #36
  * Fixed: 2.0.1

Cagebreak was not compatible with clang 15 and POSIX which was causing issues
with building under FreeBSD.

Thanks to Jan Beich for pointing this out and providing a PR.

## Issue 55

  * github issue number: N/A
  * Fixed: 2.1.0

Prior to release 2.1.0, cagebreak sometimes crased due to a null pointer
derefrence when the cursor was moved.

## Issue 56

  * github issue numner: N/A
  * Fixed: 2.1.0

Prior to release 2.1.0, the following workflow caused cagebreak to
crash:

  * Split an empty workspace.
  * Open 2 windows on the left tile.
  * exchangeright
  * focus left
  * cycle views

The reason for this is that the ipc send event did not handle
focussing the background correctly when cycling.

## Issue 57

  * github issue number: #39
  * Fixed: 2.1.0

maxhbr pointed out that there was a spelling mistake in the
SPDX-License-Identifier.

## Issue 58

  * github issue number: #40
  * Fixed: 2.1.0

Prior to version 2.1.0, the logic behind configuration file loading was
broken. This had the effect, that the default configuration file was not
loaded when the user-specific config file was not present, instead
leading to a termination of cagebreak.

## Issue 59

  * github issue number: N/A
  * Fixed: 2.1.0

Prior to release 2.1.0 `meson install` did not work perfectly.

## Issue 60

  * github issue number: N/A
  * Fixed: 2.1.2

In the `merge_output_configs` function, when copying the properties of
one config to another, the `angles` element of an input structure was
being copied to the `status` element of the resultant structure.

## Issue 61

  * github issue number: N/A
  * Fixed: 2.1.2

The script introduced with 2.1.1 `scripts/install-development-environment` had some
missing dependencies. This has been resolved and tested with bare arch containers.

## Issue 62

  * github issue number: #46
  * Fixed: 2.2.0

When the readable flag of the IPC socket was set with 0 readable bytes
available, cagebreak entered an infinite loop which caused high CPU
usage before the first event was sent over the IPC socket.

## Issue 63

  * github issue number: N/A
  * Fixed: 2.2.0

A file path in the environment-variables unit test was wrongly set,
causing this test to fail.

## Issue 64

  * github issue number: N/A
  * Fixed: 2.2.0

Cagebreak did not update the pointer focus when focussing the
background. This means that the first pointer event after focussing the
background was sent to the previously focussed window.

Steps to reproduce:

  * Open an application and hover the mouse over a clickable area.
  * Switch to an empty desktop using a keybinding without moving the
    mouse.
  * Click the mouse without moving it.
  * Observe that the element in the previously focussed window was
    clicked.

## Issue 65

  * github issue number: N/A
  * Fixed: 2.2.1

Up until cagebreak 2.2.0, the configuration for the calibration matrix of
a libinput device was not propagated correctly when internally copying
the configuration. This meant that effectively, the calibration matrix
configuration was a NoOp. Starting with version 2.2.1, the calibration
matrix can be set as documented.

Thanks to Oliver Friedmann for providing a pull request.

## Issue 66

  * github issue number: #65
  * Fixed 2.2.3

In Cagebreak 2.2.2 the compatible wlroots versions were wrongly specified.
To stop this from reoccurring, we have added a check to our release checks.

## Issue 67

  * github issue number: #66
  * Fixed 2.2.3

In Cagebreak 2.2.2 dual monitors were mirrored instead of extended, changing
established behaviour.

## Issue 68

  * github issue number: #38
  * Fixed: 2.3.0

After extensive discussion of how to deal with turning off outputs, the
output command was ammended by two settings (permanent & peripheral).
See man pages for details.

Thanks to sodface for the great cooperation.

## Issue 69

  * github issue number: #68
  * Fixed: 2.3.0

The config man page (5) was badly formatted up to version 2.3.0.

Particularly unescaped html tags caused not all information to be
shown in the web browser view of our repo.

## Issue 70

  * github issue number: #62
  * Fixed: 2.3.0

The README contained a few non-intuitive phrases up to version 2.3.0.

To alleviate this, the following changes were made:

- Rephrase README.md (for more clarity)
- Add CONTRIBUTING.md (to remove dev detail from README)
- Add PR template (to present relevant info during a PR)

## Issue 71

  * github issue number: #60
  * Fixed: 2.3.0

Cagebreak up to 2.3.0 did not have configuration for the message location.

This has been added through the initiative of unsigned-enby to improve
accessibility.

## Issue 72

  * github issue number: N/A
  * Fixed: 2.3.0

A Cagebreak version before 2.3.0 broke the gamma toolkit used to tune the
monitor colors.

## Issue 73

  * github issue number: #73
  * Fixed: 2.3.1

After 2.3.0 cagebreak did not build on FreeBSD. Since the fix was trivial,
this is fixed in 2.3.1. Thanks to Jan Beich for pointing this out.

## Issue 74

  * github issue number: #71
  * Fixed. 2.3.0

Before 2.3.0 there was an issue with rendering fullscreen applications, particularly
under setups with multiple outputs applications spanned both outputs. Output ordering
also did not work as advertised.

## Issue 75

  * github issue number: #84
  * Fixed. 3.0.0

In 2.4.0, -std=c23 was required by the meson build script to compile cagebreak even
though the code itself can be compiled with -std=c11. Thanks to Jan Beich for pointing
this out.

## Issue 76

  * github issue number: N/A
  * Fixed. 3.0.0

Before 3.0.0, opening a popup window could lead to a crash of cagebreak under certain
circumstances.

## Issue 77

  * github issue number: N/A
  * Fixed: 3.0.1

Before 3.0.1 there was a bug in the popup positioning code, where popups where positioned
relative to the workspace instead of the view.

