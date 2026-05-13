project('fake-power-sensor', 'cpp',
  version: '1.0',
  meson_version: '>=0.60.0',
  default_options: [
    'warning_level=2',
    'werror=false',
    'cpp_std=c++23',
  ],
)

sdbusplus_dep = dependency('sdbusplus')
boost_system_dep = dependency('boost', modules: ['system'])

executable(
  'fake-power-sensor',
  'src/main.cpp',
  dependencies: [sdbusplus_dep, boost_system_dep],
  install: true,
)
