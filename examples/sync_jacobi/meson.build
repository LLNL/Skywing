sync_jacobi_exe = executable(
  'sync_jacobi',
  ['sync_jacobi.cpp'],
  dependencies : [skywing_core_dep]
)

conf_data = configuration_data()
conf_data.set('sync_jacobi_exe', sync_jacobi_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
