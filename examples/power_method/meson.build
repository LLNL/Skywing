power_method_exe = executable(
  'power_method',
  ['power_method.cpp'],
  dependencies : [skywing_core_dep]
)

conf_data = configuration_data()
conf_data.set('power_method_exe', power_method_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
