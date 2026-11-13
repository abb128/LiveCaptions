# Translation worker

`live-translate-overlay.py` runs as a background worker when either translation option is enabled. It reads LiveCaptions text over D-Bus and sends translated text back to LiveCaptions; it does not create a second caption window.

The main app's **Preferences → Translation** page refreshes the Argos catalog, lists target languages, and downloads the selected model. The same page contains the caption settle delay and minimum word count controls. Word translation on hover is immediate. Model files are stored in the user's data directory and are not kept in this repository.

The worker requires GTK 3/PyGObject and Argos Translate in the same runtime as LiveCaptions. The Flatpak also needs network permission to refresh the catalog and download a selected package. Translation runs locally and does not send caption text to an online service.

The Flatpak bundles Argos Translate and its pinned runtime dependencies, but no translation language packages. Users download a language pair from **Preferences → Translation**; Argos stores it in the app's user data. The Argos wheel is installed with `--no-deps` because SpaCy and Stanza are optional sentence-splitting backends; `argos-stanza-optional.patch` makes the Stanza import optional in Argos 1.11.0, while MiniSBD remains the default backend. MiniSBD may fetch its small sentence-boundary model on first translation. The dependency list is in `build-aux/flatpak/requirements-argos.txt`, with generated, checksummed sources in `build-aux/flatpak/python3-argos-runtime-dependencies.json`.
