cagebreak-socket(7) "Version 2.0.0" "Cagebreak Manual"

# NAME

*cagebreak-socket* — Cagebreak socket

# SYNOPSIS

*ipc-socket-capable-tool \$CAGEBREAK_SOCKET*

# DESCRIPTION

The cagebreak socket is an ipc socket.

The socket accepts cagebreak commands as input (see *cagebreak-config(5)* for more information).

Events are provided as output as specified in this man page.

## EVENTS

*foo <arg>*
	Set x of y - <[x|y|z]> are foos
	between 0 and 1.
	This is a full sentence.

```
# Comment
example
```

## SECURITY

The socket is restricted to the user for reading, writing and execution.

## EXAMPLES

*nc -U $CAGEBREAK_SOCKET*

*ncat -U $CAGEBREAK_SOCKET*

# SEE ALSO

*cagebreak(1)*
*cagebreak-config(5)*

# BUGS

See GitHub Issues: <https://github.com/project-repo/cagebreak/issues>

# LICENSE

Copyright (c) 2022 The Cagebreak authors

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
