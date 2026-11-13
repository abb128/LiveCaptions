livecaptions_sources = [
  'main.c',
  'livecaptions-window.c',
  'livecaptions-welcome.c',
  'livecaptions-settings.c',
  'livecaptions-translation-settings.c',
  'livecaptions-translation-service.c',
  'livecaptions-translation-view.c',
  'livecaptions-application.c',
  'audiocap.c',
  'audiocap-pa.c',
  'audiocap-pw.c',
  'asrproc.c',
  'line-gen.c',
  'profanity-filter.c',
  'window-helper.c',
  'history.c',
  'livecaptions-history-window.c',
  'dbus-interface.c'
]

cc = meson.get_compiler('c')

livecaptions_deps = [
  dependency('libadwaita-1', version: '>= 1.0'),
  #dependency('libpipewire-0.3', version: '>=0.3.41'),
  dependency('libpulse'),
  dependency('x11'),

  cc.find_library('m', required: false),
  april_lib
]

gnome = import('gnome')

livecaptions_sources += gnome.compile_resources('livecaptions-resources',
  'livecaptions.gresource.xml',
  c_name: 'livecaptions'
)

executable('livecaptions', livecaptions_sources,
  dependencies: livecaptions_deps,
  install: true,
)

install_data('../extras/live-translate-overlay.py',
  install_dir: get_option('bindir'),
  rename: 'live-translate-overlay',
  install_mode: 'rwxr-xr-x',
)

install_data('../extras/livecaptions_argos.py',
  install_dir: get_option('bindir'),
)
