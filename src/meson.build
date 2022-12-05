livecaptions_sources = [
  'main.c',
  'livecaptions-window.c',
  'livecaptions-welcome.c',
  'livecaptions-settings.c',
  'livecaptions-application.c',
  'audiocap.c',
  'audiocap-pa.c',
  'audiocap-pw.c',
  'asrproc.c',
  'line-gen.c'
]

cc = meson.get_compiler('c')

livecaptions_deps = [
  dependency('libadwaita-1', version: '>= 1.0'),
  dependency('libpipewire-0.3', version: '>=0.3.41'),
  dependency('libpulse'),

  cc.find_library('m', required: false),
  april_lib
  #dependency(meson.project_source_root() / 'april-asr' / 'build' /  'libaprilasr.pc'),
  #dependency(meson.project_source_root() / 'april-asr' / 'lib' /  'libonnxruntime.pc')
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
