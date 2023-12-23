# Cagebreak: A Wayland Tiling Compositor

[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/6532/badge)](https://bestpractices.coreinfrastructure.org/projects/6532) [![Packaging status](https://repology.org/badge/tiny-repos/cagebreak.svg)](https://repology.org/project/cagebreak/versions) [![AUR package](https://repology.org/badge/version-for-repo/aur/cagebreak.svg?minversion=2.3.0)](https://repology.org/project/cagebreak/versions)

## Quick Introduction

Cagebreak is a [Wayland](https://wayland.freedesktop.org/) tiling compositor
based on [Cage](https://github.com/Hjdskes/cage) and inspired by [ratpoison](https://www.nongnu.org/ratpoison/).

### Purpose

This project provides a successor to ratpoison for Wayland.
However, this is no reimplementation of ratpoison.

#### New Features, Bugs and Contact Information

You can [open an issue](https://github.com/project-repo/cagebreak/issues/new)
or write an e-mail (See [SECURITY.md](SECURITY.md) for details.).

The Roadmap section outlines our plans.

#### Compatibility & Development Distribution

Cagebreak supports [Arch Linux](https://archlinux.org/) and uses the libraries
and versions from extra and core at the time of release.
Most other setups work with a bit of luck.

### Quick Installation

This assumes Arch Linux:

1. Use the [cagebreak PKGBUILD](https://aur.archlinux.org/packages/cagebreak).
2. Add an example config such as [config](examples/config) to `$USER/.config/cagebreak/config`
3. Execute cagebreak like any other binary.

See the [ArchWiki](https://wiki.archlinux.org/title/Cagebreak#Getting_started) for
details on getting started and the documentation for everything else.

### Documentation

  * the man pages: [cagebreak](man/cagebreak.1.md), [configuration](man/cagebreak-config.5.md) & [socket](man/cagebreak-socket.7.md)
  * the [README](README.md), [FAQ](FAQ.md) & [SECURITY.md](SECURITY.md)

#### What's new?

Check the [Changelog](Changelog.md).

### Uninstallation

`pacman -R cagebreak` should be sufficient.

### Contributing

  * [Open an issue](https://github.com/project-repo/cagebreak/issues/new) and state your idea.
    We will get back to you.
  * Ask before you open a pull request. We might not accept your code and
    it would be sad to waste the effort.
  * Respect the [Code of Conduct](CODE_OF_CONDUCT.md) (To date, we never
    had to intervene - Keep it that way!)

### Name

Cagebreak is based on [Cage](https://github.com/Hjdskes/cage), a Wayland kiosk
compositor. Since it breaks the kiosk into tiles the name
Cagebreak seemed appropriate.

## Installation

On Arch Linux, just use the PKGBUILDs from the [AUR](https://aur.archlinux.org/):

  * Using [cagebreak](https://aur.archlinux.org/packages/cagebreak), Cagebreak is
    compiled on the target system (since release 1.3.0)
  * Using [cagebreak-bin](https://aur.archlinux.org/packages/cagebreak-bin),
    the pre-built binaries are extracted to
    appropriate paths on the target system (since release 1.3.2)

See [cagebreak-pkgbuild](https://github.com/project-repo/cagebreak-pkgbuild) for details.

### Obtaining Source Code

There are different ways to obtain cagebreak source:

  * [git clone](https://github.com/project-repo/cagebreak) (for all releases)
  * [download release asset tarballs](https://github.com/project-repo/cagebreak/releases) (starting at release 1.2.1)

#### Verifying Source Code

There are corresponding methods of verifying that you obtained the correct code:

  * our git history includes signed tags for releases
  * release assets starting at release 1.2.1 contain a signature for the tarball

## Building Cagebreak

You can build Cagebreak with the [meson](https://mesonbuild.com/) build system. It
requires wayland, wlroots and xkbcommon to be installed. Note that Cagebreak is
developed against the latest tag of wlroots, in order not to constantly chase
breaking changes as soon as they occur.

Simply execute the following steps to build Cagebreak:

```
$ meson setup build
$ ninja -C build
```

#### Release Build

By default, this builds a debug build. To build a release build, use `meson
setup build --buildtype=release`.

##### Xwayland Support

Cagebreak comes with compile-time support for XWayland. To enable this,
first make sure that your version of wlroots is compiled with this
option. Then, add `-Dxwayland=true` to the `meson` command above. Note
that you'll need to have the XWayland binary installed on your system
for this to work.

##### Man Pages

Cagebreak has man pages. To use them, make sure that you have `scdoc`
installed. Then, add `-Dman-pages=true` to the `meson` command.

### Running Cagebreak

You can start Cagebreak by running `./build/cagebreak`. If you run it from
within an existing X11 or Wayland session, it will open in a virtual output as
a window in your existing session. If you run it in a TTY, it'll run with the
KMS+DRM backend. Note that a configuration file is required. For more
configuration options, see the man pages.

Please see `example_scripts/` for example scripts and a basis to customize
from.

#### Usage Philosophy

Cagebreak was originally built to suit the needs of its creators. This section outlines
how we intended some parts of cagebreak and might ease learning how to use cagebreak a
little bit. Please note that this does not replace the man pages or the FAQ.
Also, this is in no way intended as a guide on how cagebreak must be used but rather
as a source of inspiration and explanations for certain particularities.

1. Cagebreak is keyboard-based. Everything regarding cagebreak can be done
   through the keyboard and it is our view that it should be. This does not mean
   that pointers, touchpads and such are not available for the few applications
   that do require them.

2. Cagebreak is a tiling compositor. Every view takes up as much screen space
   as possible. We believe this is useful, as only very few programs are typically
   necessary to complete a task. To manage multiple tasks concurrently, we use
   workspaces.

3. Each task deserves its own workspace. Any given task (the sort of thing you
   might find in your calendar or on your todo list) probably requires very few
   views and ideally, these take up as much of the screen as possible.

> Combining 2. and 3. might look like this in practice:

>  * Task 1: Edit introduction section for paper on X
>  * Task 2: Coordinate event with person Y

>  * split screen vertically
>  * open web browser or pdf viewer to read literature
>  * focus next
>  * open editor
>  * change to a different workspace
>  * split screen vertically
>  * open calendar application
>  * focus next
>  * open chat application

> Now each task has its own workspace and switching between tasks is possible
> by switching between workspaces.

> Note that, for example by using the socket, more advanced setups are
> possible. But the user is warned that excessive tweaking eats into the work
> to be done.

4. Use keybindings and terminal emulators for the right purpose. Given the
   philosophy outlined above you probably launch the same few programs very
   often and others are very rarely used. We believe that commonly
   used programs should have their own keybindings together with the most
   important cagebreak commands. All the rarely used programs should be launched
   from a terminal emulator as they probably require special flags, environment
   variables and file paths anyway.

> In practice this means thinking about the applications and cagebreak commands
> you use and taking your keyboard layout into account when defining keybindings for
> your individual needs.

5. Cagebreak can't do everything, but with scripting you can do most things.
   Through the socket and with a bit of scripting, you can use the internal state
   of cagebreak in combination with cagebreak commands and the full power of
   a scripting language of your choice to do almost whatever you want.

> Example scripts can be found in the repository under `example_scripts/`.

## Roadmap

Cagebreak plans to do or keep doing the following things
in the future:

  * React to all issues.
  * Add or modify features, which the authors find convenient or important.
  * Improve the [OpenSSF Best Practices Badge Program](https://bestpractices.coreinfrastructure.org/en) level

## Governance

Cagebreak is managed by project-repo.

Project-repo is a pseudonym of at least two individuals acting
as benevolent dictators for the project by the others mutual consent.

The individuals comprising project-repo are not otherwise associated
by payment from any organisation or grant.

For all intents and purposes consider project-repo as a single
benevolent dictator for life that happens to occupy at least two brains.

### Roles

There are members of project-repo and those who are not.

There are no specific roles forced unto anyone.

### Bus Factor

The Bus Factor is a measure of how many people have to be
incapacitated for a project to be unable to continue.

The current bus factor for Cagebreak is: 1

Project-repo could still react to issues (even confidential
e-mails) and fix easier issues if any one individual were
incapacitated.

However, not all aspects of the code or release engineering
are fully resilient to the loss of any one individual.

We strive to improve the Bus Factor to at least two in all
aspects of Cagebreak.

### Governance Issues

Anyone can use the information in [SECURITY.md](SECURITY.md) to
contact the members of project-repo and bring governance
issues to their attention.

## Bugs

For any bug, please [create an issue](https://github.com/project-repo/cagebreak/issues/new) on
[GitHub](https://github.com/project-repo/cagebreak).

Fixed bugs are to be assigned a number and summarized inside Bugs.md for future reference
independent of github, in case this service is unavailable.

For other means of contacting the Cagebreak authors and for security issues
see [SECURITY.md](SECURITY.md).

## Accessibility

  * We use text input/output to interact with the user whenever possible. For
    example, sending text-based commands to the cagebreak sockets allows
    one to change every configurable feature of cagebreak.
  * Color is displayed but never a vital part to operating cagebreak.
  * Text size can be increased and background color adjusted using text commands.
  * There is no screen reader support per se but using a screen reader on socket output
    would work and cagebreak does not preclude the use of a screen reader
    for any software run with it.

## Contributors

  * Aisha Tammy
    * [make man pages optional](https://github.com/project-repo/cagebreak/pull/4), released
      in 1.6.0 with slight modifications
  * Oliver Friedmann
    * [Add output scaling](https://github.com/project-repo/cagebreak/pull/34), released
      in 2.0.0 with slight modifications
    * [Fix: calibration matrix](https://github.com/project-repo/cagebreak/pull/49),
      released in 2.2.1 with slight modifications
  * Tom Greig
    * Fix bug in merge_output_configs in 2.1.2

## License

Copyright (c) 2020-2023 The Cagebreak authors
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
