project('epd-wm', 'c',
  version: '0.1.1',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=3',
    'werror=false',
  ],
)

add_project_arguments(
  [
    '-DWLR_USE_UNSTABLE',
    '-Wall',
    '-Wundef',
    '-Wno-unused-parameter',
  ],
  language: 'c',
)

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'c')
endif

cc = meson.get_compiler('c')

is_freebsd = host_machine.system().startswith('freebsd')
if is_freebsd
  add_project_arguments(
    [
      '-Wno-format-extra-args',
      '-Wno-gnu-zero-variadic-macro-arguments',
    ],
    language: 'c'
  )
endif

wlroots        = dependency('wlroots', version: '>= 0.7.0', fallback: ['wlroots', 'wlroots'])
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
wayland_server = dependency('wayland-server')
pixman         = dependency('pixman-1')
xkbcommon      = dependency('xkbcommon')
math           = cc.find_library('m')

wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')
wayland_scanner = find_program('wayland-scanner')
wayland_scanner_server = generator(
  wayland_scanner,
  output: '@BASENAME@-protocol.h',
  arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
)

server_protocols = [
  [wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
]

server_protos_headers = []

foreach p : server_protocols
  xml = join_paths(p)
  server_protos_headers += wayland_scanner_server.process(xml)
endforeach

server_protos = declare_dependency(
  sources: server_protos_headers,
)

wlroots_has_xwayland = cc.get_define('WLR_HAS_XWAYLAND', prefix: '#include <wlr/config.h>', dependencies: wlroots) == '1'
if not wlroots_has_xwayland
  error('Cannot build epd-wm with XWayland support: wlroots has been built without it')
endif

conf_data = configuration_data()

epd_wm_sources = [
  'epd_wm.c',
  'epd/epd_driver.c',
  'epd/epd_backend.c',
  'epd/epd_output.c',
  'utils/pgm.c',
  'wm/idle_inhibit_v1.c',
  'wm/output.c',
  'wm/seat.c',
  'wm/view.c',
  'wm/xdg_shell.c',
  'wm/xwayland.c',
]

epd_wm_headers = [
  configure_file(input: 'config.h.in',
    output: 'config.h',
    configuration: conf_data),
  'epd/epd_driver.h',
  'epd/epd_backend.h',
  'epd/epd_output.h',
  'utils/pgm.h',
  'hacks/wlr_backend_multi.h',
  'hacks/wlr_utils_signal.h',
  'wm/idle_inhibit_v1.h',
  'wm/output.h',
  'wm/seat.h',
  'wm/server.h',
  'wm/view.h',
  'wm/xdg_shell.h',
  'wm/xwayland.h',
]

executable(
  meson.project_name(),
  epd_wm_sources + epd_wm_headers,
  dependencies: [
    server_protos,
    wayland_server,
    wlroots,
    xkbcommon,
    pixman,
    math,
  ],
  install: true,
)

epd_demo_sources = [
  'epd/epd_demo.c',
  'epd/epd_driver.c',
  'utils/pgm.c',
]

epd_demo_headers = [
  configure_file(input: 'config.h.in',
    output: 'config.h',
    configuration: conf_data),
  'epd/epd_driver.h',
  'utils/pgm.h',
]

executable(
  'epd-demo',
  epd_demo_sources + epd_demo_headers,
  dependencies: [
    server_protos,
    wayland_server,
    wlroots,
    xkbcommon,
    pixman,
    math,
  ],
  install: true,
)

summary = [
	'',
	'epd-wm @0@'.format(meson.project_version()),
	'',
]
message('\n'.join(summary))
