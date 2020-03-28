# Cagebreak: A Wayland Tiling Compositor Inspired by Ratpoison

This is Cagebreak, a Wayland tiling compositor. The goal of this project is to
provide a successor to ratpoison for wayland users. However, this is
no reimplementation of ratpoison. Should you like to know if a feature
will be implemented, open an issue or get in touch with the development team.

This README is only relevant for development resources and instructions. For a
description of Cagebreak and installation instructions for end-users, please see
the man page supplied in the `man/` directory and the [Wiki](https://github.com/project-repo/cagebreak/wiki/).

Cagebreak is based on [Cage](https://github.com/Hjdskes/cage), a Wayland kiosk
compositor.

## Experimenting with Cagebreak

Cagebreak is currently being developed under arch linux and uses the libraries
as they are obtained through pacman. However, cagebreak should also work on
other distributions given the proper library versions.

You can build Cagebreak with the [meson](https://mesonbuild.com/) build system. It
requires wayland, wlroots and xkbcommon to be installed. Note that Cagebreak is
developed against the latest tag of wlroots, in order not to constantly chase
breaking changes as soon as they occur.

Simply execute the following steps to build Cagebreak:

```
$ meson build
$ ninja -C build
```

### Release Build

By default, this builds a debug build. To build a release build, use `meson
build --buildtype=release`.

### Xwayland Support

Cagebreak comes with compile-time support for XWayland. To enable this,
first make sure that your version of wlroots is compiled with this
option. Then, add `-Dxwayland=true` to the `meson` command above. Note
that you'll need to have the XWayland binary installed on your system
for this to work.

### Running Cagebreak

You can start Cagebreak by running `./build/cagebreak`. If you run it from
within an existing X11 or Wayland session, it will open in a virtual output as
a window in your existing session. If you run it in a TTY, it'll run with the
KMS+DRM backend. For more configuration options, see the man pages.

## Contributing to Cagebreak

Cagebreak is currently developed to fit the needs of its creators. Should you desire
to implement a feature, please let us know in advance by opening an issue. However,
the feature set is intentionally limited (i.e. we removed support for a desktop
background) and will continue to be so in the future.

Nonetheless, don't be intimidated by the (slightly lengthy) release checklist or any other
part of this file. Do what you can, open an issue and we will collaborate
toward a solution.

### Branching strategy

All features are to be developed on feature branches, named after the feature.

There exists a branch `development` to which all reasonable feature branches
are merged for final testing.

Once `development` is ready for a release, meaning that the release checklist is fulfilled,
it is merged into `master`, creating a new release, which is tagged and signed.

Merging into master is always done by

```
git checkout development
git pull origin development
git checkout master
git merge --squash development
git tag -u keyid version HEAD
git tag -v version
git push --tags origin master
```

and a log message roughly describing the features is added in the commit.

In the past, our git history did not always reflect this scheme.

### Releases

Release checklist

  * [ ] Cursory testing
    * [ ] libfuzzer testing
  * [ ] Version Number
    * [ ] -v flag
    * [ ] git tag
  * [ ] Relevant Documentation
    * [ ] New features documented
      * [ ] man page
      * [ ] wiki
    * [ ] New configuration documented
      * [ ] man page
      * [ ] wiki
    * [ ] Changelog in README
    * [ ] Document fixed bugs in Bugs.md
    * [ ] Update hashes of the binary
    * [ ] Update signature of the binary
  * [ ] Signature
  * [ ] Branching Strategy

### Signing Key

All releases are signed by at least one of the following collection of
keys.

  * E79F6D9E113529F4B1FFE4D5C4F974D70CEC2C5B
  * 4739D329C9187A1C2795C20A02ABFDEC3A40545F

Should we at any point retire a key, we will only replace it with keys signed
by at least one of the above collection.

Should we at any point have official mail addresses, their keys will be signed by
a valid key noted above.

The full public keys can be found in `keys/` along with any revocation certificates.

### Reproducible Builds

Cagebreak offers reproducible builds given the exact library versions specified
in `meson.build`. Should the versions not match, a warning will be emitted. We have
decided on this compromise to allow flexibility and security. In general we will
adapt the versions to the packages available under archlinux at the time of
release.

#### Reproducible Build Instructions

All hashes and signatures are provided for the following build instructions.

```
meson build -Dxwayland=true --buildtype=release
ninja -C build
```

#### Hashes for Builds

For every release after 1.0.5, hashes will be provided.

1.0.6

  * sha 256: 875d77ae0d1266ace899b143ca738e6adc514ea26b2dae58a0ce5989139149f2
  * sha 512: 066829ed30b299a21ef74d5d9c7f3ff8021b877ac18a141a0aa77aae1acf880305743eedf546cba0aa06acc53d26750bf989b7fb029f0ea5ac01b3804288ab88

#### GPG Signatures

For every release after 1.0.5, a GPG signature will be provided in `signatures`.

The current signature is called `cagebreak.sig`, whereas all older signatures
will be named after their release version.

### Fuzzing

Along with the project source code, a fuzzing framework based on `libfuzzer` is
supplied. This allows for the testing of the parsing code responsible for reading
the `cagebreak` configuration file. When the `libfuzzer` is available (please
use the `clang` compiler to enable it), building the fuzz-testing software can
be enabled by passing `-Dfuzz=true` to meson. This generates a `build/fuzz/fuzz-parse`
binary according to the `libfuzzer` specifications. Further documentation on
how to run this binary can be found [here](https://llvm.org/docs/LibFuzzer.html).

## Bugs

For any bug, please [create an
issue](https://github.com/project-repo/cagebreak/issues/new) on
[GitHub](https://github.com/project-rep/cagebreak).

Fixed bugs are to be assigned a number and summarized inside Bugs.md for future reference
independent of github, in case this service is unavailable.

## Changelog

### Release 1.0.0

Adds basic tiling window manager functionality to cage.

## License

Please see [LICENSE](https://github.com/project-repo/cagebreak/blob/master/LICENSE)
