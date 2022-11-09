collective_count_exe = executable(
  'collective_count',
  ['collective_count.cpp'],
  dependencies : [skywing_core_dep]
)

conf_data = configuration_data()
conf_data.set('collective_count_exe', collective_count_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
