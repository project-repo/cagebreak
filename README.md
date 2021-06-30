# Cagebreak: A Wayland Tiling Compositor Inspired by Ratpoison

This is Cagebreak, a Wayland tiling compositor. The goal of this project is to
provide a successor to ratpoison for Wayland users. However, this is
no reimplementation of ratpoison. Should you like to know if a feature
will be implemented, open an issue or get in touch with the development team.

For documentation of Cagebreak, please see
the man pages for [cagebreak](man/cagebreak.1.md) and cagebreak
[configuration](man/cagebreak-config.5.md) and the
[Wiki](https://github.com/project-repo/cagebreak/wiki/).

Cagebreak is based on [Cage](https://github.com/Hjdskes/cage), a Wayland kiosk
compositor.

Cagebreak is currently being developed under Arch Linux and uses the libraries
as they are obtained through pacman. However, cagebreak should also work on
other distributions given the proper library versions.

## Installation

If you are using archlinux, just use the PKGBUILDs from the aur:

  * Using the `cagebreak` package, Cagebreak is compiled on the target system (since release 1.3.0)
  * Using `cagebreak-bin` package, the pre-built binaries are extracted to the appropriate paths on the target system (since release 1.3.2)

See [cagebreak-pkgbuild](https://github.com/project-repo/cagebreak-pkgbuild) for details.

### Obtaining Source Code

There are different ways to obtain cagebreak source:

  * git clone (for all releases)
  * download release asset tarballs (starting at release 1.2.1)

### Verifying Source Code

There are corresponding methods of verifying that you obtained the correct code:

  * our git history includes signed tags for releases
  * release assets starting at release 1.2.1 contain a signature for the tarball

### Building Cagebreak

You can build Cagebreak with the [meson](https://mesonbuild.com/) build system. It
requires wayland, wlroots and xkbcommon to be installed. Note that Cagebreak is
developed against the latest tag of wlroots, in order not to constantly chase
breaking changes as soon as they occur.

Simply execute the following steps to build Cagebreak:

```
$ meson build
$ ninja -C build
```

#### Release Build

By default, this builds a debug build. To build a release build, use `meson
build --buildtype=release`.

#### Xwayland Support

Cagebreak comes with compile-time support for XWayland. To enable this,
first make sure that your version of wlroots is compiled with this
option. Then, add `-Dxwayland=true` to the `meson` command above. Note
that you'll need to have the XWayland binary installed on your system
for this to work.

#### Man Pages

Cagebreak has man pages. To use them, make sure that you have `pandoc`
installed. Then, add `-Dman-pages=true to the `meson` command.

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

### Branching Strategy and Versioning

All features are to be developed on feature branches, named after the feature.

There exists a branch `development` to which all reasonable feature branches
are merged for final testing.

Once `development` is ready for a release, meaning that the release checklist is fulfilled,
it is merged into `master`, creating a new release, which is tagged and signed.

All releases are tagged according to [semantic versioning](https://semver.org) guidelines.

In the past, our git history did not perfectly reflect this scheme.

### Releases

The release checklist must be completely fulfilled in one run for a release to
occur. Once any failure occurs the entire checklist must be completed from scratch.

  * [ ] `git checkout development`
  * [ ] `git pull origin development`
  * [ ] `git push origin development`
  * [ ] `ninja -C build clang-format` makes no changes
  * [ ] New version number determined according to [semantic versioning](https://semver.org) guidelines
  * [ ] Relevant Documentation completed
    * [ ] New features
      * [ ] man pages
        * [ ] man/cagebreak
        * [ ] man/cagebreak-config
        * [ ] Set EPOCH to release day in man generation in meson.build
      * [ ] wiki
      * [ ] README.md Changelog for major and minor releases but not patches
    * [ ] Fixed bugs documented in Bugs.md
      * [ ] Include issue description from github
  * [ ] Testing
    * [ ] Manual testing
    * [ ] Libfuzzer testing
  * [ ] Version Number
    * [ ] meson.build
    * [ ] git tag
    * [ ] man pages
  * [ ] meson.build reproducible build versions are current archlinux libraries and gcc
  * [ ] Cagebreak is reproducible on multiple machines
  * [ ] Documented reproducible build artefacts
    * [ ] Hashes of the artefacts in README.md
    * [ ] Renamed previous signatures
    * [ ] Created gpg signature of the artefacts
      * [ ] `gpg --detach-sign -u keyid cagebreak`
      * [ ] `gpg --detach-sign -u keyid cagebreak.1`
      * [ ] `gpg --detach-sign -u keyid cagebreak-config.5`
  * [ ] `git add` relevant files
  * [ ] `git commit`
  * [ ] `git push origin development`
  * [ ] Determined commit and tag message (Start with "Release version_number\n\n")
    * [ ] Mentioned fixed Bugs.md issues ("Fixed Issue n")
    * [ ] Mentioned other important changes
  * [ ] `git checkout master`
  * [ ] `git merge --squash development`
  * [ ] `git commit` and insert message
  * [ ] `git tag -u keyid version HEAD` and insert message
  * [ ] `git tag -v version` and check output
  * [ ] `git push --tags origin master`
  * [ ] `git checkout development`
  * [ ] `git merge master`
  * [ ] `git push --tags origin development`
  * [ ] `git archive --prefix=cagebreak/ -o release_version.tar.gz tags/version .`
  * [ ] Create release-artefacts_version.tar.gz
    * [ ] `mkdir release-artefacts_version`
    * [ ] `cp build/cagebreak release-artefacts_version/`
    * [ ] `cp build/cagebreak.sig release-artefacts_version/`
    * [ ] `cp build/cagebreak.1 release-artefacts_version/`
    * [ ] `cp build/cagebreak.1.sig release-artefacts_version/`
    * [ ] `cp build/cagebreak-config.5 release-artefacts_version/`
    * [ ] `cp build/cagebreak-config.5.sig release-artefacts_version/`
    * [ ] `cp LICENSE release-artefacts_version/`
    * [ ] `export SOURCE_DATE_EPOCH=$(git log -1 --pretty=%ct) ; tar --sort=name --mtime= --owner=0 --group=0 --numeric-owner -czf release-artefacts_version.tar.gz release-artefacts_version`
  * [ ] Checked archive
    * [ ] tar -xvf release_version.tar.gz
    * [ ] cd cagebreak
    * [ ] meson build --buildtype=release
    * [ ] ninja -C build
    * [ ] gpg --verify ../signatures/cagebreak.sig build/cagebreak
    * [ ] cd ..
    * [ ] rm -rf cagebreak
  * [ ] `gpg --detach-sign -u keyid release_version.tar.gz`
  * [ ] `gpg --detach-sign -u keyid release-artefacts_version.tar.gz`
  * [ ] Upload archives and signatures as release assets

### Reproducible Builds

Cagebreak offers reproducible builds given the exact library versions specified
in `meson.build`. Should a version mismatch occur, a warning will be emitted. We have
decided on this compromise to allow flexibility and security. In general we will
adapt the versions to the packages available under Arch Linux at the time of
release.

There are reproducibility issues up to and including release `1.2.0`. See
`Issue 5` in [Bugs.md](Bugs.md).

#### Reproducible Build Instructions

All hashes and signatures are provided for the following build instructions.

```
meson build -Dxwayland=true -Dman-pages=true --buildtype=release
ninja -C build
```

#### Hashes for Builds

For every release after 1.0.5, hashes will be provided.

For every release after 1.7.0, hashes will be provided for man pages too.

1.7.4 cagebreak

  * sha 256: c7442db906239b13f8d09e91cbe01f9e47b2a2c75bbb4b52adefd15dfbfa820f
  * sha 512: 9c36ce706f31e1b8afacd8f9cd216c460d66030e0b2736a3dd929902b0d5fc52c5e69dbf6787205b8c495d960e00b6e7f633da86ab6e31613f1b4a9fd47c9f0e

1.7.4 cagebreak.1

  * sha 256: 3dacc521e5e9b963cceeefeed6a10104c7c415d86826a72788aba5b33ce320f1
  * sha 512: 0a4c4fbff09dd403fed61056ae3211ad2b4370b4e9ed07806fc22624e5312b4a25e469dc0eef1ca4556cbb8efabb0127e7519df79852e110faa98fb17ca377de

1.7.4 cagebreak-config.5

  * sha 256: ed22fed1818172f31d3fb3efbe5e66b49f135cb2367b1c85209fd880ff7532ab
  * sha 512: cc634c63962ae50b4de20a3aad8ca0970175b53156690d69e6a69fd8f370ad20d84db783cc7e94c61d4d4d89f1205d43622690e58afba549ed57ddad009e3bac

1.7.3 cagebreak

  * sha 256: 8adb4911d9603fec5b8eb4f67a09be95b8f0b37e50d4ab579191da78df18f70b
  * sha 512: eddaecd7b577b9fe77b1ed4d341074f7f860967568392d0479543ea8860b4289df3eb5645885e18e35260b14843663ae50f98103f8016e857237a397632322a6

1.7.3 cagebreak.1

  * sha 256: bcc623e42c86ab65bc2f8cb7e4377ca0d816ddc281527b7caeded62753e943b8
  * sha 512: b4290e1040bd6a299d478ad6b695de75cf030d0d274340b43d3ed4382cf6877f824cd4ed9d7020bfb55afe276c62de7fc07c3fbee6f6485c6440a024115afe12

1.7.3 cagebreak-config.5

  * sha 256: b9789b6abd473e9afb33d4e2c7822f7d8f3c10e9eb3fb70c47b713f6331c626a
  * sha 512: c49e01cfc4a4ed272f054bea8f2845f0e3f1609cf85d1a0ce14c1b67dc207df153b11bc20ceef09c855d467f0e013e54623ce9f70c217c4278607fc54d0e3206

1.7.2 cagebreak

  * sha 256: 40f4642e8e8caee338055079b046f6e0b790c87226ee307b27a64fcb2a6c6472
  * sha 512: 22c5f1986ca4262a7c08a6c03cffe36eaedd8c3c5e27e12baf12a90f94aea088ac93ee107770f162fc70148be687f6394b8fd64ad3fe14414bdf63b55b33798e

1.7.2 cagebreak.1

  * sha 256: 3253e592c40c78fd3733c761163acc2efaaf5865627cf51fd33cc0bc5d9cc6e7
  * sha 512: 901305e166c10aa7e4a26b721a69e0ba4779f4f8ee690551438ceadae65929f13949457872b9397dd46c5b7bad6a6f2638d6fd6df9bc5567c914c798cbf3e9a4

1.7.2 cagebreak-config.5

  * sha 256: 148ec4d80dc9a86485fe77f8daf1f7fa4cb3d1b6aaa709bb012005583779e1f7
  * sha 512: d5732572e47dd07078ac2f6d589522e6007188fc05ac79520c93442212d5c0341459f135191b88c608a75b2f56c2de4065566bd869fbc24e475936d32a54a7f8

1.7.1 cagebreak

  * sha 256: 5a40797e016a2fa32b5bb70a056b35c482bef09f1795bbaf0188dbadaf8197c3
  * sha 512: 2ef327b63ac4651f23517be8d5cb8fbec99bd889f52f8abfd62a09ca6e6c0060347ab8d60e4917d39edd36db34597a673239967533f14457733acb97f6c29a7f

1.7.1 cagebreak.1

  * sha 256: 0301f04254ae6e7ada7e22bbe7aee022ec0793fbce434a396868258e6f1ea4cf
  * sha 512: f0c0c5de30be907e9ba5c9cfa0bca8cef3e94c0b5f52de9e2c3681c85aa957f75e92b5e26bb894c3a5e251295cfe7a1470653cc7dc05564d7d91c6eb43b7409a

1.7.1 cagebreak-config.5

  * sha 256: 6882330a77f3ff98ea642f47b4090b38d542abf0b7be3f226486c5614899efc6
  * sha 512: a572f97836e4ccc6d67aa21c7f4aaebe06adb066c70038b670ec1ef267e3750764cb53e96b8d7bc24c7f993f24c0cfabee183307b45006d049f7416559bcaca4

1.7.0

  * sha 256: 5af11b84e61a82a1a276e8f91f6ebc6baafd2c98d22be3da1d01734cc9d55faa
  * sha 512: 45474115fb4c28fe1ee91ad54706bfe521b6e70bd8ea619b94a3f50bae426891bf6a0387aa9b2f7f6d18349280c4e84973e4e433d50c090e5a764c64fa67795d

1.6.0

  * sha 256: aef473eae73454429afb158d66a7bd9aec75ef915845b7508de5448820357aa8
  * sha 512: 17889bc1af9f343598a5ee599b25e702987dd0bd905a9566874e8a0094c57168e413938624949fa214e6e382cbcf89d7342292b44012cd8e692947f6c01a960a

1.5.1

  * sha 256: 9c6df4b94c180f5657cb424383aa2bf3aca063c8e7b40af0497be1ef7bdb858a
  * sha 512: 219f753347ae64a4dd593568797438742e9ddae380c729598d8eda863080a6ed4e5086d735fc348c48328bb795a5a5b57b0421be5dd45755744d895b24312627

1.5.0

  * sha 256: 22010e2916493b0d7cfb08c2196f12589509ce11010a7107aed13ddbfd9709cc
  * sha 512: 707d6cc01326ba32402f9fb402ff8aceaeaba63ca6ba2eb157e5a4c83123515a3a74da960813479db303eddc2e7f79f39b241dff05b89d47d72d5f1d20ca6d37

1.4.4

  * sha 256: c6e08e19feb17f768e136cf02f5692ef121ff289763dc60ef8678103803d3307
  * sha 512: aebf27fa9c24f93dfd0b7c96536c253e2ea603d6746c40e8de2163e06367bc57ac4e22d6c07dd1e0963cce5b18b460d373e15b5c12a5a42ae6b70787971960c9

1.4.3

  * sha 256: 5e29cbaf7c0c5ca74f71a5605bb3d6e302e051798401fee9a79419720b8d8e95
  * sha 512: 8004865f8603648d9ac9bb700f3ee040e6516c222ad3d783b34219c8fa3a7bdbba6abb74fe92c4d0bd4dbbce0ec4d08be12297a8bad5032baeb589eec557e24e

1.4.2

  * sha 256: 27efb9328cf9cab1f81e66627696baf8c7cc2c372339a2890f955b40f3a7c396
  * sha 512: afcdbbce0753aff4770a88ddbd7df5687a7de0c77de3a2d296f85a8b8e01813212be6a0b69548aadef58e0e7da19ffea71c4f3448572656d4b19ff7f2f2fb70a

1.4.1

  * sha 256: c3e0ccceaf1078b91071c40b0ccb7c4f8e53ae38b05ee6637f9f39f7f6ece2cb
  * sha 512: fd77f39fc7a7e5376d8505a61d643ed4d8f4cfbbf38d70cf9ef6989645e8cc6cb9d4b1024d241dc689b6fd927a405c97b9c918b6994b8f9ab7406646f37b952d

1.4.0

  * sha 256: 14aa93d890c62cc763b3b546a51ad8560479d5e91fa9f949858ae6bc785e80be
  * sha 512: 88f93b49792684c38553b65d2296a0e502a059814a0cfff30d05c89958870d6f66d67aac2074cfd307bad82b0d72d5c4944822a61a68f665dc161407db365fbc

1.3.4

  * sha 256: ea9f634b8f6e2b9f191745a25abff4969eb7ea19cb3186b79a6138904aedeb1c
  * sha 512: 9094f9c92acbff8fd95d38e7458c5184804f8913fa8c405bc2b6a0a220fe478b06cfeadbae41bd5ef84bde8d5642c249df9dada953e39c25926aa650c30a5309

1.3.3

  * sha 256: 801851ceb52afac333a1decdd236ddef55555f8b36f865e8b3a831975a496f0d
  * sha 512: ce64dd56ab99bba0a83414fecc82d85322e1ca5ca3a2ca77bdc7055b2dd10fd5e20e3ff62809a696d1b04ff0046fed88e2d7538aa447099d7c2d7df8e627b2e0

1.3.2

  * sha 256: 2cefab73b8d5902b30adb0dda1bd292af2ee808d232abcc4b2a2802d522d5232
  * sha 512: 3789efde2cb6f629c77738dd6c8638646b345d5272868aa9b244b2b19c53e6a0f5911d34744e3a9ff6253c73b2b11f6714f37ae86cc21d6979e0c8539315d659

1.3.1

  * sha 256: 71be224cd99d20da4f039675ec5c48213dd14ef027b804501fa3241886b2b08b
  * sha 512: d094b786e5a74b01699997a85747943e6de3544cc903dd1c5dba449c66bd925b6378de800d3734969dabf8a20c7004dc3402e3c59577db09ce107b68a1f7bd21

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

Due to errors in the release process, the releases 1.7.1 and 1.7.2 did not include the release
signatures in the appropriate folder of the git repository. However, signatures were provided
as release-artefacts at the time of release. The signatures were introduced into the
repository with 1.7.3. The integrity of cagebreak is still the same because the signatures were
provided as release-artefacts (which were themselves signed) and the hashes in README.md
are part of a signed release tag.

#### Signing Keys

All releases are signed by at least one of the following collection of
keys.

  * E79F6D9E113529F4B1FFE4D5C4F974D70CEC2C5B
  * 4739D329C9187A1C2795C20A02ABFDEC3A40545F
  * 7535AB89220A5C15A728B75F74104CC7DCA5D7A8
  * 827BC2320D535AEAD0540E6E2E66F65D99761A6F
  * A88D7431E5BAAD0B6EAE550AC8D61D8BD4FA3C46
  * 8F872885968EB8C589A32E9539ACC012896D450F
  * 896B92AF738C974E0065BF42F2576BD366156BB9
  * AA927AFD50AF7C6810E69FE8274F2C605359E31B

Should we at any point retire a key, we will only replace it with keys signed
by at least one of the above collection.

We registered project-repo.co and added mail addresses after release `1.3.0`.

We now have a mail address and its key is signed by signing keys. See Security
Bugs for details.

The full public keys can be found in `keys/` along with any revocation certificates.

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

#### Caveat

Currently, there are memory leaks which do not seem to stem from our code but rather
the code of wl-roots or some other library we depend on. We are working on the problem.
In the meantime, add `-Db_detect-leaks=0` to the meson command to exclude memory leaks.

## Bugs

For any bug, please [create an
issue](https://github.com/project-repo/cagebreak/issues/new) on
[GitHub](https://github.com/project-rep/cagebreak).

Fixed bugs are to be assigned a number and summarized inside Bugs.md for future reference
independent of github, in case this service is unavailable.

### Security Bugs

Should you want to get in touch with the developers of cagebreak to report a
security vulnerability or a different issue confidentially, contact
`cagebreak @ project-repo . co`.

We will try to respond to everything that is not obvious spam.

Please encrypt your email with the appropriate GPG key found in `keys/`.

  * B15B92642760E11FE002DE168708D42451A94AB5
  * F8DD9F8DD12B85A28F5827C4678E34D2E753AA3C

Note that the keys are signed by cagebreak signing keys.

If you want us to respond via GPG-encrypted mail, please include your own
public key or provide the fingerprint and directions to obtain the key.

## Changelog

### Release 1.0.0

Adds basic tiling window manager functionality to cage.

### Release 1.1.0

Unifies commands and actions. See Issue 4 in Bugs.md.

### Release 1.2.0

Adds output configuration as described in the man pages.

### Release 1.3.0

Adds IPC as described in the man pages.

### Release 1.4.0

Adds close command for windows as described in the man pages.

### Release 1.5.0

Adds options to disable or enable outputs. See Issue 22 in Bugs.md and Issue #2 on github.

### Release 1.6.0

Adds support for non-build dependencies and an option for builds without pandoc.

### Release 1.7.0

Improve window ordering.

## Contributors

  * Aisha Tammy
    * [make man pages optional](https://github.com/project-repo/cagebreak/pull/4), released
      in 1.6.0 with slight modifications

## License

Please see [LICENSE](https://github.com/project-repo/cagebreak/blob/master/LICENSE)
