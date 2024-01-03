# FAQ for Development

## How is cagebreak adjusted to a new wlroots version?

There are three steps:

1. Try to compile Cagebreak with the new wlroots.
2. Fix the compiler errors one-by-one using the wlroots
   changelog for reference (https://gitlab.freedesktop.org/wlroots/wlroots/-/releases).
3. Debug until Cagebreak works again.

## How do I add a new test?

1. Add a shell script to `test/`
2. Optionally add test configs to `test/testing-configurations/`
3. Make sure the files have shebang, copyright and SPDX License identifiers
   (use the other files for reference)
4. Add the test to `meson.build` as in the example below.
5. Add paths and env vars as shown in the other tests
6. Make sure the test is added to the correct suite
   (check out CONTRIBUTING.md for details)

```
test('Scan-build (static analysis)', find_program('test/scan-build'), env : [ ''.join('MESONCURRENTCONFIGDIR=', meson.current_source_dir()) ], suite: 'devel-long')
```

## How do I add a new script?

1. Add a shell script to `scripts/`
2. Make sure the files have shebang, copyright and SPDX License identifiers
   (use the other files for reference)
3. Add the script to CONTRIBUTING.md
4. Add the script to meson.build as shown below

```
run_target('create-sigs',
  command : ['scripts/create-signatures', get_option('gpg_id')])
```

## How do I add an example script?

Extrapolate from the examples in the `example_scripts` directory.

The script should be executable with the .sh library file in the same directory.

License, contributors etc. should be appropriate.

Shellcheck must pass on any script (judicious use of shellcheck pragmas is allowed but
discouraged).

## How do I add a new gpg key?

1. Check which gpg key versions are currently valid.
2. Generate keys with incremented numbers/emails/dates/passphrase.
   * Use 4096 Bit RSA Keys
3. Sign the new keys with at least one then-old signing key.
4. Genereate new cagebreak@project-repo.co key
5. Sign the new mail key with the new signing keys.
6. Generate new pkgbuild key.
7. Sign the pkgbuild key with the new signing keys.
8. Add public keys to `keys/`.
9. Update meson_options.txt
10. Update [all man pages](../manuals.md), [CONTRIBUTING](../CONTRIBUTING.md) & [SECURITY.md](../SECURITY.md)
11. Update the pkgbuild repo with the new key (key and readme).
12. Securely distribute private keys and revocation certificates as per the internal wiki.
