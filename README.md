# Cagebreak: A Tiling Wayland Compositor

[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/6532/badge)](https://bestpractices.coreinfrastructure.org/projects/6532) [![Packaging status](https://repology.org/badge/tiny-repos/cagebreak.svg)](https://repology.org/project/cagebreak/versions) [![AUR package](https://repology.org/badge/version-for-repo/aur/cagebreak.svg?minversion=2.4.0)](https://repology.org/project/cagebreak/versions)

[![Contact](img/mail.svg)](SECURITY.md) [![Manuals](img/manuals.svg)](manuals.md) [![FAQ](img/faq.svg)](FAQ.md) [![CONTRIBUTING](img/contributing.svg)](CONTRIBUTING.md) [![ArchWiki](img/archwiki.svg)](https://wiki.archlinux.org/title/Cagebreak) [![AUR](img/aur.svg)](https://aur.archlinux.org/packages?O=0&K=cagebreak)

## Introduction

Cagebreak provides a [ratpoison](https://www.nongnu.org/ratpoison/)-inspired, [cage](https://github.com/Hjdskes/cage)-based, tiling [Wayland](https://wayland.freedesktop.org/) compositor.

  * Bugs, Contact Information and New Features:
    * [Open an issue](https://github.com/project-repo/cagebreak/issues/new).
    * Write an e-mail (See [SECURITY.md](SECURITY.md)).
    * The Roadmap below outlines our plans.
  * Compatibility & Development Distribution:
    * Cagebreak supports [Arch Linux](https://archlinux.org/) and uses the library
      versions from extra and core at the time of release.
      Most other setups work with a [bit of luck](https://repology.org/project/cagebreak/versions).
  * Quick Installation (on ArchLinux, more [here (ArchWiki)](https://wiki.archlinux.org/title/Cagebreak)):
    1. Use the [cagebreak PKGBUILD](https://aur.archlinux.org/packages/cagebreak).
    2. Add an example config such as [config](examples/config) to `$USER/.config/cagebreak/config`
    3. Execute cagebreak like any other binary.
  * Documentation:
    * [man pages](manuals.md)
    * [README](README.md), [FAQ](FAQ.md) & [SECURITY.md](SECURITY.md)
  * What's new?: [Changelog](Changelog.md)
  * Uninstallation: `pacman -R cagebreak`
  * Contributing:
    * [Open an issue](https://github.com/project-repo/cagebreak/issues/new) and state your idea.
      We will get back to you.
    * Ask before you open a pull request. We might not accept your code and
      it would be sad to waste the effort.
    * Respect the [Code of Conduct](CODE_OF_CONDUCT.md) (To date, we've never
      had to intervene. - Keep it that way!)
  * Name: Cagebreak breaks the kiosk of [Cage](https://github.com/Hjdskes/cage) into tiles, hence the name.

## Installation

On [Arch Linux](https://archlinux.org/), use the [AUR](https://aur.archlinux.org/) [PKGBUILDs](https://wiki.archlinux.org/title/PKGBUILD):

  * Using [cagebreak](https://aur.archlinux.org/packages/cagebreak), Cagebreak is
    built on the target system (since release 1.3.0)
  * Using [cagebreak-bin](https://aur.archlinux.org/packages/cagebreak-bin),
    pre-built binaries are extracted
    to the target system (since release 1.3.2)

See [cagebreak-pkgbuild](https://github.com/project-repo/cagebreak-pkgbuild) for details.

You may check out other distros here:

[![Packaging status](https://repology.org/badge/vertical-allrepos/cagebreak.svg)](https://repology.org/project/cagebreak/versions)

### Obtaining Source Code

There are different ways to obtain cagebreak source:

  * [git clone](https://github.com/project-repo/cagebreak) (for all releases)
  * [download release asset tarballs](https://github.com/project-repo/cagebreak/releases) (starting at release 1.2.1)

#### Verifying Source Code

There are ways to verify that you obtained the correct source (See `keys/`
and [CONTRIBUTING](CONTRIBUTING.md)):

  * signature for the tarball of release assets starting at release 1.2.1
  * signed tags for releases in the git history

## Building Cagebreak

Cagebreak uses the [meson](https://mesonbuild.com/) build system.

Cagebreak is developed against the latest tag of wlroots, so as not to constantly
chase breaking changes.

Execute the following commands to build Cagebreak:

```
$ meson setup build -Dxwayland=true -Dman-pages=true --buildtype=release
$ ninja -C build
```

### Man Pages

Remove `-Dman-pages=true` to disable man page generation.

To generate man pages, make sure that you have [scdoc](https://archlinux.org/packages/extra/x86_64/scdoc/) installed.

### Release Build

To obtain a debug build, remove `--buildtype=release`.

The release build is reproducible under conditions outlined in [CONTRIBUTING](CONTRIBUTING.md).

### Xwayland Support

To build Cagebreak without XWayland support, remove `-Dxwayland=true`.

To use XWayland make sure that your version of wlroots is compiled with
XWayland support.

You'll need to have [XWayland](https://archlinux.org/packages/extra/x86_64/xorg-xwayland/) installed for XWayland support to work.

## Running Cagebreak

```
$ ./build/cagebreak
```

If you run Cagebreak within an existing X11 or Wayland session, it will
open in a virtual output as a window in your existing session.

If you run it in a TTY, it'll run with the KMS+DRM backend.

Note that a configuration file is required. For more configuration options, see the [man pages](manuals.md).

Please see `example_scripts/` for a basis to customize from.

### Usage Philosophy

Cagebreak was built to suit the needs of its creators. This outlines
how we intended some parts of cagebreak and might ease learning how to use cagebreak a
little bit. Please note that this does not replace the [man pages](manuals.md) or the [FAQ](FAQ.md).
Also, this is not intended as a guide on how cagebreak must be used but
as a source of inspiration and explanation for certain particularities.

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

## Resilience

To become more resilient to outages of GitHub, we have created a [website](https://cagebreak.project-repo.co).

It is not possible to open issues on the website directly, use the mail contact
if GitHub is down.

The following links may be useful:

  * [Artefacts mirror](https://cagebreak.project-repo.co/release-artefacts.html)
  * [PKGBUILD depending on the mirror](https://cagebreak.project-repo.co/cb-red-pkgb/PKGBUILD)
  * [binary PKGBUILD depending on the mirror](https://cagebreak.project-repo.co/cb-red-bin-pkgb/PKGBUILD)
  * [cagebreak repo mirror](https://cagebreak.project-repo.co/cagebreak.git)
  * [cagebreak-bin repo mirror](https://cagebreak.project-repo.co/cagebreak-pkgbuild.git)

## Roadmap

Cagebreak plans to do or keep doing the following things
in the future:

  * React to all issues.
  * Add or modify features, which the authors find convenient or important.
  * Improve the [OpenSSF Best Practices Badge Program](https://bestpractices.coreinfrastructure.org/en) level.

## Governance

Cagebreak is managed by project-repo.

Consider project-repo a single benevolent dictator for life that happens
to occupy at least two brains.

Project-repo is a pseudonym of at least two individuals acting
as benevolent dictators for the project by the others mutual consent.

The individuals comprising project-repo are not otherwise associated
by payment from any organisation or grant.

### Governance Issues

Use [SECURITY.md](SECURITY.md) to contact project-repo.

### Roles

There are members of project-repo and those who are not.

There are no specific roles forced unto anyone.

### Bus Factor

The current bus factor for Cagebreak is: 1

The Bus Factor is a measure of how many people have to be
incapacitated for a project to be unable to continue.

Project-repo could react to issues (even confidential
e-mails) and fix easier issues if any one individual were
incapacitated.

However, not all aspects of the code or release engineering
are fully resilient to the loss of any one individual.

We strive to increase the bus factor to at least 2 in all
aspects.

## Accessibility

  * We use text input/output to interact with the user whenever possible. For
    example, sending text-based commands to the cagebreak sockets allows
    one to change every configurable feature of cagebreak.
  * Color is displayed but never a vital part to operating cagebreak.
  * Text size can be increased and background color adjusted using text commands.
  * There is no screen reader support per se but using a screen reader on socket output
    would work and cagebreak does not preclude the use of a screen reader
    for any software run with it.

## Bugs

[Open an issue](https://github.com/project-repo/cagebreak/issues/new) is you find something.

See [SECURITY.md](SECURITY.md) for other means of contacting the Cagebreak authors and security issues.

Fixed bugs are assigned a number and summarized in [Bugs.md](Bugs.md) for future reference.

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
  * sodface
    * Add a screenshot example script in 2.3.0
  * Luca Kennedy (unsigned-enby)
    * [Add option to configure the anchor position of messages](https://github.com/project-repo/cagebreak/pull/61) in 2.3.0

## License

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
