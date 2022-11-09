project('skywing', 'cpp',
  version: '0.1',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
    'werror=true'
  ]
)

python = import('python').find_installation('python3')

# This isn't a useful warning and is impossible to get correct when specializing
# things in the std namespace as anything can either be a struct or class
if meson.get_compiler('cpp').has_argument('-Wmismatched-tags')
  add_project_arguments(
    '-Wno-mismatched-tags',
    language : 'cpp'
  )
endif

add_project_arguments(
  '-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_' + get_option('logging_level').to_upper(),
  language : 'cpp'
)

# use_ns3 = get_option('use_ns3')
# message('using ns3: ' + use_ns3.to_string())

build_tests = get_option('build_tests')
if build_tests
  catch2_dep = subproject('catch2').get_variable('catch2_dep')
endif

# if use_ns3
#   ns3_dep = subproject('ns3').get_variable('ns3_dep')
# endif

thread_dep = dependency('threads')
spdlog_dep = subproject('spdlog').get_variable('spdlog_dep')
gsl_dep = declare_dependency(
  include_directories : include_directories('subprojects/gsl/include')
)

capnproto_dep = dependency('capnp', version : '>=0.8.0')

use_helics = get_option('use_helics')
if use_helics
  helics_dep = dependency('helicsSharedLib')
endif

subdir('generated_files/generated')

skywing_core_inc = include_directories('skywing', 'generated_files')
skywing_core_internal_dep = declare_dependency(
  include_directories : skywing_core_inc,
  dependencies : [thread_dep, capnproto_dep, spdlog_dep, gsl_dep],
  sources : [generated_sources]
)

subdir('skywing/skywing_core')

skywing_core_dep = declare_dependency(
  dependencies : skywing_core_internal_dep,
  link_with : skywing_core_lib
)

subdir('skywing/skywing_mid')
skywing_upper_dep = declare_dependency(
  dependencies : skywing_core_dep
)

if build_tests
  subdir('tests')
endif

build_examples = get_option('build_examples')
build_lc_examples = get_option('build_lc_examples')
if build_examples
  subdir('examples')
endif
