# Cagebreak: A Wayland Tiling Compositor Inspired by Ratpoison

This is Cagebreak, a Wayland tiling compositor. The goal of this project is to
provide a successor to ratpoison for Wayland users. However, this is
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

### GCC and -fanalyzer

Cagebreak should compile with any reasonably new gcc or clang. Consider
a gcc version of at least [10.1](https://gcc.gnu.org/gcc-10/changes.html) if
you want to get the benefit of the brand-new
[-fanalyzer](https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html)
flag. However, this new flag sometimes produces false-postives and we
selectively disable warnings for affected code segments as described below.

Meson is configured to set `CG_HAS_FANALYZE` if `-fanalyzer` is available.
Therefore, to maintain portability, false-positive fanalyzer warnings are to be
disabled using the following syntax:

```
#if CG_HAS_FANALYZE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "WARNING OPTION"
#endif
```
and after

```
#if CG_HAS_FANALYZE
#pragma GCC diagnostic pop
#endif
```

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
git checkout development
git merge master
git push --tags origin development
```

and a log message roughly describing the features added in the commit is
included.

In the past, our git history did not always reflect this scheme.

### Releases

Release checklist

  * [ ] Check cage for possible code merges
  * [ ] Cursory testing
    * [ ] libfuzzer testing
  * [ ] ninja -C build clang-format
  * [ ] Version Number
    * [ ] meson.build
    * [ ] git tag
    * [ ] man pages
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
  * [ ] git archive --prefix=cagebreak/ -o release_version.tar.gz tags/version .
  * [ ] Extract, build and verify reproducibility of archive
  * [ ] gpg --detach-sign -u keyid release_version.tar.gz
  * [ ] Upload archive and signature as build assets

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
in `meson.build`. Should a version mismatch occur, a warning will be emitted. We have
decided on this compromise to allow flexibility and security. In general we will
adapt the versions to the packages available under arch linux at the time of
release.

There are reproducibility issues up to and including release `1.2.0`. See
`Issue 5` in [Bugs.md](Bugs.md).

#### Reproducible Build Instructions

All hashes and signatures are provided for the following build instructions.

```
meson build -Dxwayland=true --buildtype=release
ninja -C build
```

#### Hashes for Builds

For every release after 1.0.5, hashes will be provided.

1.3.0

  * sha 256: aecc5292f56f7250d777110cee1f11a92018a9eb2117d7d37cd51d7d3a5cbae0
  * sha 512: 1d2d220f44787a97359a32f305e33185f0369551e61a9d7066023bfa43de50849eba5bcbb0083d2dc563ac993b8d51dd733afa0adb2a823ef50ecfee5624bbc0

1.2.1

  * sha 256: 803f7667dc4997062f9ec95afcdca0cac68c82a7cf057cef83fe1ccfee33b8bc
  * sha 512: 4d6b7efea10d190a153f893123b1715f290f4b131b490f3d23f489e2dd8edbb886001282849aad9575ad2f40375bbbded59c1cb6157115fcc14dd6a2a061b103

1.2.0

  * sha 256: b3c6135833001a0ef600ea9628cca11f4130a524ebe748929d8f7bad034f2bf0
  * sha 512: c044a086a9f65b0d27d05e2f12a20fe5d77386fc5682991c13a1894a84804bcd337c9167235869661d6e6581133521bd710ef2330066a5d2e2703d1630042f3f

1.1.0

  * sha 256: fc393e225c549f893b9e21e8b904e546d5857bac1d905b3d26334c8f8a1cda11
  * sha 512: 8b94b1069e767202bcab087cb592eadc42f0453d17ee119d48760be00bc66278cf0fa60bb09308d30cb551df6a7a3e26e2ab8b29c04f0e953cc6906542cd2d6f

1.0.6

  * sha 256: 712ae9a8f17a9e589e108f0d503da203cc5eaf1c4a6ca6efb5b4c83b432ce0b8
  * sha 512: d574003023a00cfd6623aac986a5a7f397cfd0bc9114017629a8c72731b0df3977c4a31768502dfa8a6607be06930089b2ccf6ffca9b5bcd1096b7ca0aede226

#### GPG Signatures

For every release after 1.0.5, a GPG signature will be provided in `signatures`.

The current signature is called `cagebreak.sig`, whereas all older signatures
will be named after their release version.

### Fuzzing

Along with the project source code, a fuzzing framework based on `libfuzzer` is
supplied. This allows for the testing of the parsing code responsible for reading
the `cagebreak` configuration file. When `libfuzzer` is available (please
use the `clang` compiler to enable it), building the fuzz-testing software can
be enabled by passing `-Dfuzz=true` to meson. This generates a `build/fuzz/fuzz-parse`
binary according to the `libfuzzer` specifications. Further documentation on
how to run this binary can be found [here](https://llvm.org/docs/LibFuzzer.html).

Here is an example workflow:

```
rm -rf build
CC=clang meson build -Dfuzz=true -Db_sanitize=address,undefined -Db_lundef=false
ninja -C build/
mkdir build/fuzz_corpus
cp examples/config build/fuzz_corpus/
WLR_BACKENDS=headless ./build/fuzz/fuzz-parse -jobs=12 -max_len=50000 -close_fd_mask=3 build/fuzz_corpus/
```

You may want to tweak `-jobs` or add other options depending on your own setup.
We have found code path discovery to increase rapidly when the fuzzer is supplied
with an initial config file. We are working on improving our fuzzing coverage to
find bugs in other areas of the code.

## Bugs

For any bug, please [create an
issue](https://github.com/project-repo/cagebreak/issues/new) on
[GitHub](https://github.com/project-rep/cagebreak).

Fixed bugs are to be assigned a number and summarized inside Bugs.md for future reference
independent of github, in case this service is unavailable.

## Changelog

### Release 1.0.0

Adds basic tiling window manager functionality to cage.

### Release 1.1.0

Unifies commands and actions. See Issue 4 in Bugs.md.

### Release 1.2.0

Adds output configuration as described in the man pages.

### Release 1.3.0

Adds IPC as described in the man pages.

## License

Please see [LICENSE](https://github.com/project-repo/cagebreak/blob/master/LICENSE)
