# Copyright 2020 - 2023, project-repo and the cagebreak contributors
# SPDX-License-Identifier: MIT

fuzz_sources = [
  'fuzz-parse.c', 
  'fuzz-lib.c',
  ]

fuzz_headers = [
  '../parse.h',
  'fuzz-lib.h',
  ]

inc = include_directories(['..','../build/'])

fuzz_dependencies = cagebreak_dependencies
if fuzzing_engine.found()
  fuzz_dependencies += fuzzing_engine
else
  link_args = [ '-fsanitize=fuzzer', ]
endif

override_lib = shared_library('execl_override',
  [ 'execl_override.c' ],
  dependencies: [ cairo,pango,pangocairo ],
  install: false
  )

executable(
  'fuzz-parse',
  fuzz_sources + fuzz_headers + cagebreak_headers + cagebreak_sources,
  dependencies: fuzz_dependencies,
  install: false,
  include_directories: inc,
  link_args: link_args + fuzz_link_args,
  c_args: fuzz_compile_args,
  link_with: override_lib,
  )
