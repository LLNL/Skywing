if target_machine.system() == 'darwin'
  platform_specific_sources = [
    'internal/devices/socket_wrappers_osx.cpp'
  ]
elif target_machine.system() == 'linux'
  platform_specific_sources = [
    'internal/devices/socket_wrappers_linux.cpp'
  ]
else
  error('Unsupported build target "' + target_machine.system() + '"')
endif

skywing_core_lib = static_library('skywing_core',
  [
    'internal/devices/socket_communicator.cpp',
    'internal/utility/network_conv.cpp',
    'internal/capn_proto_wrapper.cpp',
    'internal/manager_waiter_callables.cpp',
    'internal/message_creators.cpp',
    'internal/reduce_group.cpp',
    # 'basic_manager_config.cpp',
    'job.cpp',
    'manager.cpp'
  ] + platform_specific_sources,
  dependencies : [skywing_core_internal_dep],
  include_directories : include_directories('include')
)
