async_jacobi_example_exe = executable(
  'async_jacobi',
  ['async_jacobi.cpp'],
  dependencies : [skywing_core_dep]
)

conf_data = configuration_data()
conf_data.set('async_jacobi_exe', async_jacobi_example_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
