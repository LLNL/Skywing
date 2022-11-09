config = configure_file(
  input : 'config.cfg',
  output : 'config.cfg',
  copy : true
)

async_iterative = executable(
  'asynchronous_iterative',
  ['asynchronous_iterative.cpp'],
  dependencies : [skywing_core_dep]
)

conf_data = configuration_data()
conf_data.set('exe', async_iterative.full_path())

configure_file(input: 'run.sh.in', output: 'run.sh', configuration: conf_data)
