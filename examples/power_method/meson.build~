mean_or_sum_exe = executable(
  'mean_or_sum',
  ['mean_or_sum.cpp'],
  dependencies : [skynet_core_dep]
)

conf_data = configuration_data()
conf_data.set('mean_or_sum_exe', mean_or_sum_exe.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
