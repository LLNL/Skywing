core_tests = {
  # Base path, test names
  'core': [
    'assorted',
    'broadcast',
    'broken_reduce',
#    'broken_subscribes',
    'disconnect',
    'heartbeat',
    'ip_subscribe',
    'publish_data_wrapper',
    'publish_multiple_values',
    'reduce_tag_bug',
    'repeat_connection',
    'self_subscribe',
    'simple_reduce',
  ],
  'core/devices': [
    'socket_communicator'
  ],

  'mid': [
    'synchronous_iterative',
    'stoppolicy_send',
    'asynchronous_iterative',
    'max_test',
    'associative_vector',
    'pubsub',
    'count_test'
  ]
}

write_to_file = ''

foreach directory, test_names : core_tests
  foreach test_name : test_names
    exe = executable(
      test_name,
      ['skywing_' + directory + '/' + test_name + '.cpp', 'catch_main.cpp'],
      dependencies : [catch2_dep, skywing_core_dep],
      include_directories : include_directories('include')
    )
    write_to_file += directory + '/' + test_name + '\0' + exe.full_path() + '\n'
  endforeach
endforeach

run_config = configure_file(
  input : 'test_runner.txt.in',
  configuration : {'run_commands': write_to_file},
  output : 'test_runner.txt'
)

script_path = meson.source_root() / 'scripts' / 'tester_script.py'

test('run_all_tests', python, args : [script_path, run_config])

configure_file(
  input : 'run_tests.sh.in',
  output : 'run_tests.sh',
  # There's no actual way to get the path from the config file...
  configuration : {'script_path' : script_path, 'config_path' : meson.build_root() / 'tests' / 'test_runner.txt'}
)
