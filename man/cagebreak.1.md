% CAGEBREAK(1) Version 1.3.0 | Cagebreak Manual

# NAME

**cagebreak** — A Wayland tiling compositor to the likes of ratpoison

# SYNOPSIS

| **cagebreak** [OPTIONS]

# DESCRIPTION

`cagebreak` is a slim, keyboard-controlled, tiling compositor for
wayland conceptually based on the X11 window manager `ratpoison`.

It allows for the screen to be split into non-overlapping tiles and,
in contrast to the original `ratpoison`, has native support for
multi-workspace operation.

All interactions between the user and `cagebreak` are done via
the keyboard. Configuration of this behaviour is specified
in the **\$XDG_CONFIG_PATH/cagebreak/config** file (See **cagebreak-config(5)**).

## OPTIONS

-h

: Display help message and exit

-r

: Rotate the output 90 degrees clockwise, can be specified up to three times

-v

: Show version number and exit

# ENVIRONMENT

`XKB_DEFAULT_LAYOUT`

: The keyboard layout to be used (See **xkeyboard-config(7)**)

`XKB_DEFAULT_MODEL`

: The keyboard model to be used (See **xkeyboard-config(7)**)

`XKB_DEFAULT_VARIANT`

: The keyboard variant to be used (See **xkeyboard-config(7)**)

`XKB_DEFAULT_RULES`

: The xkb rules to be used

`XKB_DEFAULT_OPTIONS`

: The xkb options to be used

# SEE ALSO

**cagebreak-config(5)**

# BUGS

See GitHub Issues: <https://github.com/project-repo/cagebreak/issues>

# LICENSE

Copyright (c) 2020 The Cagebreak authors
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
