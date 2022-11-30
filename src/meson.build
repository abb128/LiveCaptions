livecaptions_sources = [
  'main.c',
  'livecaptions-window.c',
  'livecaptions-settings.c',
  'livecaptions-application.c',
  'audiocap.c',
  'asrproc.c',
  'line-gen.c'
]

livecaptions_deps = [
  dependency('libadwaita-1', version: '>= 1.0'),
  dependency('libpipewire-0.3', version: '>=0.3.41'),

  dependency('/run/media/alex/EncSSD/ASR/own-cmake/build/libaprilasr.pc'),
  dependency('/run/media/alex/EncSSD/ASR/own-cmake/lib/libonnxruntime.pc')
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
