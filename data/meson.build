desktop_file = i18n.merge_file(
  input: 'net.sapples.LiveCaptions.desktop.in',
  output: 'net.sapples.LiveCaptions.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
    args: [desktop_file]
  )
endif

appstream_file = i18n.merge_file(
  input: 'net.sapples.LiveCaptions.appdata.xml.in',
  output: 'net.sapples.LiveCaptions.appdata.xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'appdata')
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validate appstream file', appstream_util,
    args: ['validate', appstream_file]
  )
endif

gnome.compile_schemas(build_by_default: true, depend_files: 'net.sapples.LiveCaptions.gschema.xml')
devenv = environment()
devenv.set('GSETTINGS_SCHEMA_DIR', meson.current_build_dir() / 'data')
meson.add_devenv(devenv)

install_data('net.sapples.LiveCaptions.gschema.xml',
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas')
)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file', compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()]
  )
endif

subdir('icons')
