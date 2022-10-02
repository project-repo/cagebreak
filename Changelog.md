# Changelog

## Release 1.0.0

Adds basic tiling window manager functionality to cage.

## Release 1.1.0

Unifies commands and actions. See Issue 4 in Bugs.md.

## Release 1.2.0

Adds output configuration as described in the man pages.

## Release 1.3.0

Adds IPC as described in the man pages.

## Release 1.4.0

Adds close command for windows as described in the man pages.

## Release 1.5.0

Adds options to disable or enable outputs. See Issue 22 in Bugs.md and Issue #2 on github.

## Release 1.6.0

Adds support for non-build dependencies and an option for builds without pandoc.

## Release 1.7.0

Improves window ordering.

## Release 1.8.0

Adds libinput configuration and virtual keyboard and pointer support.

## Release 1.9.0

- Add message functionality (as per Issue 29 in Bugs.md)
  - message <text>
  - configure_message
- Add (as per Issue 30 in Bugs.md)
  - movetoprevscreen
  - movetoscreen <n>
  - screen <n>
  - additional output functionality for monitor numbering
- Improve configuration documentation
  - Fix Issue 31 in Bugs.md (word omission)
- Improve README.md and split off some files
  - Add SECURITY.md
  - Add Hashes.md (hashes of cagebreak binaries and man pages if built reproducably)
  - Add Changelog.md (changelog for major and minor releases but not patches)
  - Add FAQ.md (updated information from the former wiki)
- Depreciate wiki
- Fix Issue 32 in Bugs.md
  - Improve release checklist to partially prevent the above issue
    in the future
- Fix Issue 33 in Bugs.md
- Fix Issue 34 in Bugs.md
- Manage gpg keys
  - Add new cagebreak gpg signing keys
  - Add new cagebreak email contact gpg key
  - Add new project-repo AUR gpg key (relevant for cagebreak-pkgbuild)

