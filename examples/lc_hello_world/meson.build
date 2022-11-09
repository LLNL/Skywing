lc_hello_world_exe = executable(
  'lc_hello_world',
  ['lc_hello_world.cpp'],
  dependencies : [skywing_core_dep]
)

conf_data = configuration_data()
conf_data.set('lc_hello_exe', lc_hello_world_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
configure_file(input: 'sbatch_script.sh.in', output: 'sbatch_script.sh', configuration: conf_data)
configure_file(input: 'generate_config_info.py', output: 'generate_config_info.py', configuration: conf_data)
configure_file(input: 'call_executable.sh.in', output: 'call_executable.sh', configuration: conf_data)