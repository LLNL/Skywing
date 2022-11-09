helics_hello_world_exe = executable(
  'helics_hello_world',
  ['helics_hello_world.cpp'],
  dependencies : [skywing_core_dep, helics_dep]
)

conf_data = configuration_data()
conf_data.set('exe', helics_hello_world_exe.full_path())
conf_data.set('helics_broker', helics_dep.get_pkgconfig_variable('exec_prefix') + '/helics_broker')

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
