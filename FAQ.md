# Frequently Asked Questions

## How do I do $thing with cagebreak?

  * Check the man pages
    * [cagebreak config](man/cagebreak-config.5.md) (probably where you find the answer)
    * [cagebreak](man/cagebreak.1.md) (command line options etc.)
  * Check the rest of this FAQ
  * [Open an issue](https://wiki.archlinux.org/title/Linux_console/Keyboard_configuration) or otherwise get in touch with the development team (See section Email Contact in [SECURITY.md](SECURITY.md)).

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

## Which further resources are available?

  * [Arch Linux Wiki entry for Cagebreak](https://wiki.archlinux.org/title/Cagebreak)
  * [Sway Wiki](https://github.com/swaywm/sway/wiki)
  * [Cage Wiki](https://github.com/Hjdskes/cage/wiki)

## This FAQ did not help me. What now?

  * Consider [opening an issue](https://github.com/project-repo/cagebreak/issues/new) on github or getting in touch with the
    development team (See section Email Contact in [SECURITY.md](SECURITY.md)).
