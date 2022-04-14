# Security

The main possibility for security bugs to occur in cagebreak is by privilege
escalation using the socket. Indeed, any program with access to the socket
immediately gains arbitrary code execution rights. As of right now, the socket
is world-writable. If you disagree with this threat model, you may contact
us via email (See section Email Contact below.) or [open an issue on github](https://github.com/project-repo/cagebreak/issues/new).

Should any problem with the github issue system arise or any other reason
for (potentially confidential) contact with the Cagebreak authors appear,
you may contact us via email (See section Email Contact below.).

## Supported Versions

The latest release always contains the latest bug fixes and features.
There are no official backports for security vulnerabilities. Builds
are reproducible under conditions outlined in [README.md](README.md).

## Bug Reports

For normal bugs you may [open an issue on github](https://github.com/project-repo/cagebreak/issues/new).

For everything else, an email contact (with gpg encryption and signature)
is available below.

## Email Contact

Should you want to get in touch with the developers of cagebreak to report
a security vulnerability or anything else via email, contact
`cagebreak @ project-repo . co`.

We will try to respond to everything that is not obvious spam.

### GPG-Encrypted Emails

If you can, please encrypt your email with the appropriate GPG key found
in `keys/` and sign your message with your own key.

* B15B92642760E11FE002DE168708D42451A94AB5
* F8DD9F8DD12B85A28F5827C4678E34D2E753AA3C
* 3ACEA46CCECD59E4C8222F791CBEB493681E8693

Note that our keys are signed by cagebreak signing keys.

If you want us to respond via GPG-encrypted email, please include your own
public key or provide the fingerprint and directions to obtain the key.

## Threat Model

Cagebreak is a wayland compositor, which is run by the user of the system
and thus has access to whichever resources this user has access to.
Cagebreak can restrict other programs in no way, because this would hamper
usability (consider a web browser unable to write a downloaded file to disk
for instance). There is no transmission of information by cagebreak other
than to the screens, ipc and potentially other documented local channels.

### STRIDE Threat List

This is not a thorough analysis, just an overview of the ways in which cagebreak
has/does not have an attack surface.

#### Spoofing

Not applicable - Using cagebreak already requires a login as a user.

#### Tampering

Not applicable - Cagebreak must allow system manipulation for user software.

#### Repudiation

Not applicable - There are no prohibited operations (See Tampering above.).
While cagebreak does send events over documented channels there is no logging
activated by default, though, of course, this can be changed by the user
by logging socket output for example.

#### Information Disclosure

Not applicable - Information disclosure over documented channels is a feature
and any software run by the user may exfiltrate any data the user has access to.

#### Denial of Service

Not applicable - Cagebreak offers functionality to terminate itself, which is
available to all user software over the socket.

#### Elevation of privilege

Software may gain arbitrary code execution rights if it has access to the
Cagebreak socket. Privilege escalation to root is unlikely since privileges
are dropped before any user input is accepted.
