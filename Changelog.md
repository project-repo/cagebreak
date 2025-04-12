# Changelog

## Release 3.0.0

BREAKING CHANGES:
- Add option to set cursor in arbitrary mode
- Add feature to remove message display

New Features:
- Add merging tiles
- Add option for splitting by percentage
- Add focus with optional tile id
- Add next with optional view id
- Add numeric only command
- Add numeric resize with tile id
- Add numeric exchange
- moveviewtotile
- moveviewtoworkspace

Fixes:
- Fix Issue 75 - 76 in Bugs.md


## Release 2.3.0

Functionality:
  * Add configuration to anchor position of messages (#60, 71 in Bugs.md).
  * Add output priorisation (#38, 68 in Bugs.md).
  * Print whether output is active in dump.

Bug Fixes:
  * Fix gamma control (72 in Bugs.md).
  * Add missing commands to example config.
  * Add message_config to dump output.

Example Scripts:
  * Add screenshot script.
  * Make example scripts standalone.

Documentation:
  * Escape html tags in config man page. (#68, 69 in Bugs.md)
  * Improve README.md (#62, 70 in Bugs.md).
  * Improve SECURITY.md.
  * Add CONTRIBUTING.md (#62, 70 in Bugs.md).
  * FAQ: Add Firefox screenshare instructions for wayland.
  * FAQ: Add wlroots downgrading instructions for workaround in case of temporary wlroots incompatibility.
  * Update copyright notices to 2024.

Development Tooling:
  * Add new gpg keys (signed by the old ones).
  * Add PR template.
  * Add dev-FAQ.
  * Remove now-unnecessary fanalyzer pragmas.

## Release 2.2.0

  * Add custom events
  * Fix Issue 62 (#46 Github)
  * Fix Issue 63 (environment variables test)
  * Fix Issue 64 (pointer focus bug)

## Release 2.1.0

  * Add -bs (bad security) option
  * Fix Issue 57 (SPDX spelling error #39)
  * Fix Issue 59 (meson install settings)
  * Fix Issue 55 (npd)
  * Fix Issue 56 (npd)
  * Fix Issue 58 (#40 config)
  * Improve release procedure
  * Add miniscule test suite
  * Adjust Cagebreak to wlroots 0.16.2
  * New GPG Keys

## Release 2.0.0

This is a major release and includes BREAKING changes!

Breaking Changes:

  * More intuitive switch of focus, next, prev and exchange (#20, 40 in Bugs.md)
    Cagebreak will probably still feel broadly the same though.
  * Remove -r flag - This is replaced by output configuration.
  * Socket disabled by default - The socket is enabled by invoking Cagebreak with the -e flag.
  * Socket permissions restricted to the user of the Cagebreak process (700)
    This may require modification of scripts interacting with the socket with
    different UIDs than the UID of the Cagebreak process.
  * Rename "Current frame" indicator to "Current tile"

Changelog:

  * Move to wlr_scene
  * Add events displaying change over the socket
    * this enables scripting tools
  * Add support for scaling outputs (thanks to Oliver Friedmann)
  * Remove -r flag
  * Add rotation to output configuration
  * Add cursor enable|disable flag
  * Add input keyboard configuration
  * Custom path flag for configuration file
  * Restrict socket permissions to the user running cagebreak (700)
  * Fix Issue #31 (resolve some terminology) 38 in Bugs.md
  * Fix Issue #30 - 39 in Bugs.md
  * Fix Issue #20 - 40 in Bugs.md
  * Fix Issue #12 - 41 in Bugs.md (dump + events over socket)
  * Fix Bugs found via scan-build static analysis (42 - 45 in Bugs.md)
  * Fix Issue #33 - 46 in Bugs.md
  * Fix Issue #32 - 47 in Bugs.md
  * Fix Issue #16 - 48 in Bugs.md
  * Fix Issue #3 - 49 in Bugs.md (disabling keybinding interpretation for specific keyboards is now possible)
  * Fix Issue #26 - 50 in Bugs.md
  * Fix Issue #7 - 51 in Bugs.md (Change cursor to square while Cagebreak is waiting for a key)
  * Fix config bug for some commands - 52 in Bugs.md
  * Fix Issue #35 - 53 in Bugs.md (update to wlroots 0.16.1)
  * Improve FAQ.md (related to some Issues)
  * Disable outputs when unable to set any mode (crashed previously)
  * Add Code of Conduct
  * Print version number on startup

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

## Release 1.8.0

Adds libinput configuration and virtual keyboard and pointer support.

## Release 1.7.0

Improves window ordering.

## Release 1.6.0

Adds support for non-build dependencies and an option for builds without pandoc.

## Release 1.5.0

Adds options to disable or enable outputs. See Issue 22 in Bugs.md and Issue #2 on github.

## Release 1.4.0

Adds close command for windows as described in the man pages.

## Release 1.3.0

Adds IPC as described in the man pages.

## Release 1.2.0

Adds output configuration as described in the man pages.

## Release 1.1.0

Unifies commands and actions. See Issue 4 in Bugs.md.

## Release 1.0.0

Adds basic tiling window manager functionality to cage.

