# Frequently Asked Questions

## How do I do a particular thing with cagebreak?

  * Check the man pages
    * [cagebreak config](man/cagebreak-config.5.md) (probably where you find the answer)
    * [cagebreak](man/cagebreak.1.md) (command line options etc.)
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
