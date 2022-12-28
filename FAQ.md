# Frequently Asked Questions

## How do I do a particular thing with cagebreak?

  * Check the man pages
    * [cagebreak config](man/cagebreak-config.5.md) (probably where you find the answer)
    * [cagebreak](man/cagebreak.1.md) (command line options etc.)
    * [cagebreak-socket](man/cagebreak-socket.7.md) (socket information, mostly useful for scritping)
  * Check the rest of this FAQ
  * [Open an issue](https://github.com/project-repo/cagebreak/issues/new) or otherwise get in touch with the development team (See section Email Contact in [SECURITY.md](SECURITY.md)).

Note that the feature set of cagebreak is intentionally limited.

## How do I remap Caps Lock?

Remapping Caps Lock globally seems to be the best option.
Follow instructions [here](https://wiki.archlinux.org/title/Linux_console/Keyboard_configuration) to achieve this (should be roughly
distribution-agnostic).

## How do I get firefox to run under wayland?

Set the following environment variables

  * `MOZ_USE_WAYLAND=1`
  * `GDK_BACKEND=wayland`

to activate wayland support for firefox.

## How do I map characters which are not on the Keyboard?

Suppose you want to use a command like `bind` for a character
which is likely not on your keyboard  like "ы" - you should write
it as "Cyrillic_yeru".

These names can be obtained by running xev and pressing the respective
key(s) (combinations).

## How do I change the size of the cursor?

The size and shape of the cursor is determined by the application, however,
most applications respect the environment variables `XCURSOR_SIZE` and
`XCURSOR_THEME`

To change the cursor size, one could use:

```
export XCURSOR_SIZE=1
```

If you are using an application which inherits its environment variables
from something outside the scope of the export (such as a daemon for
emacs might), you can usually specify your environment variables in
the applications respective configuration.

## How is Cagebreak launched?

The documentation says to start Cagebreak like any other binary.

If you want to start Cagebreak on login, you can use your shells
equivalent of a `.bash_profile`. In BASH appending cagebreak
(given that it is installed in the path) to `.bash_profile`
is fine.

If you want to start Cagebreak using systemd you could use a
service started on user login, though the development team once ran
into some issues with permissions for the services.

Please note that Cagebreak does not solve the problem of
locking the screen or login in general.

## How are applications launched in Cagebreak?

There are multiple ways to launch external programs in cagebreak.

  * Using a terminal emulator is a perfectly valid way to launch
    applications. If you want to be able to close the terminal
    you used to start the program, you may use `&disown`, which should
    be available in every shell.
  * Using the `exec` command in the cagebreak config, it is possible to
    launch programs when cagebreak is started. Moreover, as this command
    can be combined with commands such as `bind`, it can be used to
    create custom keybindings which execute external programs. (See also
    the man page `cagebreak-config`).
  * All commands mentioned previously are also available over the socket
    which allows the creation of (arbitrary) keybindings on the fly and
    a fortiori keybindings running arbitrary commands.

## Which further resources are available?

  * [Arch Linux Wiki entry for Cagebreak](https://wiki.archlinux.org/title/Cagebreak)
  * [Sway Wiki](https://github.com/swaywm/sway/wiki)
  * [Cage Wiki](https://github.com/Hjdskes/cage/wiki)

## This FAQ did not help me. What now?

  * Consider [opening an issue](https://github.com/project-repo/cagebreak/issues/new) on github or getting in touch with the
    development team (See section Email Contact in [SECURITY.md](SECURITY.md)).
