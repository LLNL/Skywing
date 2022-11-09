hello_world_exe = executable(
  'hello_world',
  ['hello_world.cpp'],
  dependencies : [skywing_core_dep]
)

conf_data = configuration_data()
conf_data.set('hello_exe', hello_world_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)