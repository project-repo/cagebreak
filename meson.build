project('cagebreak', 'c',
  version: '1.7.4',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=3',
  ],
)

add_project_arguments(
  [
    '-DWLR_USE_UNSTABLE',
    '-Wundef',
    '-Wno-unused-parameter',
  ],
  language: 'c',
)

if get_option('buildtype').startswith('debug')
  add_project_arguments(
	[
	  '-DDEBUG',
	  '-g',
	],
	language : 'c',
)
endif

cc = meson.get_compiler('c')

fuzz_compile_args = []
fuzz_link_args = []
if get_option('fuzz')
  fuzzing_engine = meson.get_compiler('c').find_library('Fuzzer', required : false)
  if fuzzing_engine.found()
	fuzz_compile_args += '-fsanitize-coverage=trace-pc-guard,trace-cmp'
  elif cc.has_argument('-fsanitize=fuzzer-no-link')
	fuzz_compile_args += '-fsanitize=fuzzer-no-link'
	fuzz_link_args += '-fsanitize=fuzzer-no-link'
  else
    error('Looks like neither libFuzzer nor -fsanitize=fuzzer-no-link is supported. Disable fuzzing using -Dfuzz=false to build without fuzzers.')
  endif
endif

is_freebsd = host_machine.system().startswith('freebsd')
if is_freebsd
  add_project_arguments(
    [
      '-Wno-format-extra-args',
      '-Wno-gnu-zero-variadic-macro-arguments',
    ],
    language: 'c'
  )
endif

wlroots        = dependency('wlroots', version: ['>=0.14.0', '< 0.15.0'])
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
wayland_server = dependency('wayland-server')
wayland_cursor = dependency('wayland-cursor')
wayland_client = dependency('wayland-client')
pixman         = dependency('pixman-1')
xkbcommon      = dependency('xkbcommon')
cairo          = dependency('cairo')
pango          = dependency('pango')
pangocairo     = dependency('pangocairo')
fontconfig     = dependency('fontconfig')
math           = cc.find_library('m')

wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')
wayland_scanner = find_program('wayland-scanner')
wayland_scanner_server = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.h',
  arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
)

server_protocols = [
  [wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
]

server_protos_headers = []

foreach p : server_protocols
  xml = join_paths(p)
  server_protos_headers += wayland_scanner_server.process(xml)
endforeach

server_protos = declare_dependency(
  sources: server_protos_headers,
)

if get_option('xwayland')
  wlroots_has_xwayland = cc.get_define('WLR_HAS_XWAYLAND', prefix: '#include <wlr/config.h>', dependencies: wlroots) == '1'
  if not wlroots_has_xwayland
    error('Cannot build Cagebreak with XWayland support: wlroots has been built without it')
  else
    have_xwayland = true
  endif
else
  have_xwayland = false
endif

fanalyzer_compile_flag=[]
if cc.has_argument('-fanalyzer')
  have_fanalyze=true
  fanalyzer_compile_flag=[ '-fanalyzer' ]
else
  have_fanalyze=false
endif

if get_option('version_override') != ''
  version = '@0@'.format(get_option('version_override'))
else
  version = '@0@'.format(meson.project_version())
endif

conf_data = configuration_data()
conf_data.set10('CG_HAS_XWAYLAND', have_xwayland)
conf_data.set10('CG_HAS_FANALYZE', have_fanalyze)
conf_data.set_quoted('CG_VERSION', version)


cagebreak_main_file = [ 'cagebreak.c', ]
cagebreak_source_strings = [
  'idle_inhibit_v1.c',
  'ipc_server.c',
  'keybinding.c',
  'workspace.c',
  'output.c',
  'parse.c',
  'render.c',
  'seat.c',
  'util.c',
  'view.c',
  'xdg_shell.c',
  'server.c',
  'message.c',
  'pango.c',
]

cagebreak_header_strings = [
  'idle_inhibit_v1.h',
  'ipc_server.h',
  'keybinding.h',
  'workspace.h',
  'output.h',
  'parse.h',
  'render.h',
  'seat.h',
  'server.h',
  'util.h',
  'view.h',
  'xdg_shell.h',
  'pango.h',
  'message.h',
]

if conf_data.get('CG_HAS_XWAYLAND', 0) == 1
  cagebreak_source_strings += 'xwayland.c'
  cagebreak_header_strings += 'xwayland.h'
endif

cagebreak_sources = [
  configure_file(input: 'config.h.in',
		 output: 'config.h',
		 configuration: conf_data),
  ]

cagebreak_headers = []

foreach source : cagebreak_source_strings
  cagebreak_sources += files(source)
endforeach

foreach header : cagebreak_header_strings
  cagebreak_headers += files(header)
endforeach
# Dependencies marked with "true" are required to have
# the version specified below in order for the build
# to be reproducible.
cagebreak_dependencies_dict = {
  'server_protos': [server_protos,true],
  'wayland_server': [wayland_server,false],
  'wayland_client': [wayland_client,true],
  'wayland_cursor': [wayland_cursor,true],
  'wlroots': [wlroots,true],
  'xkbcommon': [xkbcommon,true],
  'fontconfig': [fontconfig,true],
  'pixman': [pixman,true],
  'pango': [pango,true],
  'cairo': [cairo,true],
  'pangocairo': [pangocairo,true],
  'math': [math,true],
}

reproducible_build_versions = { 
  'server_protos': '-1',
  'wayland_server': '1.19.0',
  'wayland_client': '1.19.0',
  'wayland_cursor': '1.19.0',
  'wlroots': '0.14.0',
  'xkbcommon': '1.3.0',
  'fontconfig': '2.13.94',
  'pixman': '0.40.0',
  'pango': '1.48.6',
  'cairo': '1.17.4',
  'pangocairo': '1.48.6',
  'math': '-1'
}

cagebreak_dependencies = []

foreach name, dep : cagebreak_dependencies_dict
  cagebreak_dependencies += dep[0]
endforeach

foreach name, dep : cagebreak_dependencies_dict
  if reproducible_build_versions[name] != '-1'
    if dep[1] == false
      if dep[0].version() < reproducible_build_versions[name]
        warning('The installed version of "' + name + '" on your machine (' + dep[0].version() + ') is older than the one used to generate the binary specified in the README section "Reproducible Builds" (' + reproducible_build_versions[name] + '). It is recommended to use an up-to-date version for compiling cagebreak.'
        )
      endif
    elif reproducible_build_versions[name] != dep[0].version()
      warning('The installed version of "' + name + '" on your machine (' + dep[0].version() + ') differs from the one used to generate the binary specified in the README section "Reproducible Builds" (' + reproducible_build_versions[name] + '). Cagebreak does not guarantee a reproducible build for this configuration.'
      )
      break
    endif
  endif
endforeach

reproducible_build_compiler = 'gcc'
reproducible_build_compiler_version = '11.1.0'

if cc.get_id() != reproducible_build_compiler
  warning('The compiler "' + cc.get_id() + '" differs from the one used to generate to binary specified in the README section "Reproducible Builds" (' + reproducible_build_compiler + ').')
elif cc.version() != reproducible_build_compiler_version
  warning('The version of ' + cc.get_id() + ' (' + cc.version() + ') differs from the one used to generate the binary specified in the README section "Reproducible Builds" ' + reproducible_build_compiler_version + '.')
endif

executable(
  meson.project_name(),
  cagebreak_main_file + cagebreak_sources + cagebreak_headers,
  dependencies: cagebreak_dependencies,
  install: true,
  link_args: fuzz_link_args+fanalyzer_compile_flag,
  c_args: fuzz_compile_args,
)

if get_option('man-pages')
  scdoc = find_program('scdoc')
  sh = find_program('sh')
  mandir1 = join_paths(get_option('mandir'), 'man1')
  mandir5 = join_paths(get_option('mandir'), 'man5')

  cagebreak_man = custom_target('cagebreak_man',
    output : 'cagebreak.1',
    input : 'man/cagebreak.1.md',
	capture : true,
    command : [sh, '-c', 'export SOURCE_DATE_EPOCH=1625064321 ; @0@ < @INPUT@'.format(scdoc.path())],
    install: true,
    install_dir: mandir1
    )

  cagebreak_man = custom_target('cagebreak_config_man',
    output : 'cagebreak-config.5',
    input : 'man/cagebreak-config.5.md',
	capture : true,
    command : [sh, '-c', 'export SOURCE_DATE_EPOCH=1625064321 ; @0@ < @INPUT@'.format(scdoc.path())],
    install: true,
    install_dir: mandir5
    )
endif

if get_option('fuzz')
  subdir('fuzz')
endif

summary = [
	'',
	'Cagebreak @0@'.format(version),
	'',
	'    xwayland: @0@'.format(have_xwayland),
	''
]
message('\n'.join(summary))
