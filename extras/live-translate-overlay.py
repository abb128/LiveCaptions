#!/usr/bin/env python3

# ============================================================
# FORCE XWAYLAND
# ============================================================

import os
import sys

if "--worker" in sys.argv or "--settings" in sys.argv:
    # The integrated worker and package manager should use the compositor's
    # native backend. Forcing X11 makes the settings window fail in Wayland
    # Flatpak sessions which do not expose an X11 socket.
    os.environ.pop("GDK_BACKEND", None)
else:
    # Keep the original standalone overlay's XWayland positioning behavior.
    os.environ["GDK_BACKEND"] = "x11"

import argparse
import json
import logging
import shutil
import subprocess
import threading
from collections import OrderedDict

import gi

gi.require_version("Gtk", "3.0")

from gi.repository import Gtk, Gio, GLib, Gdk, Pango

from livecaptions_argos import ArgosLanguagePackages


# ============================================================
# LOGGING
# ============================================================

logging.getLogger().setLevel(logging.ERROR)
logging.getLogger("stanza").setLevel(logging.ERROR)
logging.getLogger("argostranslate").setLevel(logging.ERROR)


# ============================================================
# LIVE CAPTIONS DBUS
# ============================================================

DBUS_SERVICE = "net.sapples.LiveCaptions"
DBUS_PATH = "/net/sapples/LiveCaptions/External"
DBUS_INTERFACE = "net.sapples.LiveCaptions.External"
DBUS_SIGNAL = "TextStream"
TRANSLATION_PATH = "/net/sapples/LiveCaptions/Translation"
TRANSLATION_INTERFACE = "net.sapples.LiveCaptions.Translation"
HOVER_SIGNAL = "HoverWord"


# ============================================================
# LANGUAGE
# ============================================================

SOURCE_LANGUAGE = "en"
DEFAULT_TARGET_LANGUAGE = "ru"
ARGOS_LANGUAGE_PACKAGES = ArgosLanguagePackages(
    GLib.get_user_data_dir(),
    SOURCE_LANGUAGE,
)


# ============================================================
# CONFIG
# ============================================================

CONFIG_DIR = os.path.expanduser(
    "~/.config/live-translate"
)

CONFIG_FILE = os.path.join(
    CONFIG_DIR,
    "settings.json"
)


DEFAULT_SETTINGS = {
    "target_language": DEFAULT_TARGET_LANGUAGE,
    "show_dual_subtitles": True,
    "translate_on_hover": False,

    # Allow brief caption updates to settle before translating.
    "settle_ms": 90,

    "min_words": 2,

    # 0.0 = полностью прозрачно
    # 1.0 = полностью непрозрачно
    "background_opacity": 0.78,

    "font_size": 13,

    # Компактно, близко к Live Captions
    "window_width": 620,
    "window_height": 52,

    "bottom_margin": 110,
}


MIN_WIDTH = 360
MIN_HEIGHT = 44

MAX_LINES = 2


class LiveTranslateOverlay:

    def __init__(self, headless=False, settings_only=False):

        self.headless = headless
        self.settings_only = settings_only
        self.translation_label = None
        self.source_event_box = None
        self.source_label = None
        self.bus = None

        try:
            self.app_settings = Gio.Settings.new("net.sapples.LiveCaptions")
        except Exception:
            self.app_settings = None

        self.settings = self.load_settings()

        # ====================================================
        # TRANSLATION STATE
        # ====================================================

        self.latest_text = ""
        self.latest_lines = ("", "")
        self.last_seen_text = ""
        self.last_translated_text = ""
        self.last_published_translation = ""
        self.line_translation_cache = OrderedDict()
        self.displayed_source_lines = ("", "")
        self.displayed_translation_lines = ("", "")
        self.displayed_target_language = None

        self.translation_running = False
        self.pending_translation = False
        self.translation_lock = threading.Lock()
        self.translation_execution_lock = threading.Lock()
        self.translator = None
        self.missing_language_notice_shown = False

        self.hover_timeout_id = None
        self.hover_word = ""
        self.hover_translation = ""
        self.hover_target_language = None
        self.hover_translation_running = False
        self.pending_hover_word = None

        self.settle_id = None

        # ====================================================
        # LANGUAGE PACK STATE
        # ====================================================

        self.available_language_pairs = {}
        self.installed_target_languages = set()
        self.language_catalog_loading = False
        self.language_installing = False
        self.language_combo = None
        self.language_status_label = None
        self.language_action_button = None
        self.language_refresh_button = None

        # ====================================================
        # UI STATE
        # ====================================================

        self.positioned_once = False
        self.window = None
        self.settings_window = None

        # Один CSS provider на всё приложение
        self.css_provider = None
        if not self.headless:
            self.css_provider = Gtk.CssProvider()
            screen = Gdk.Screen.get_default()
            if screen is not None:
                Gtk.StyleContext.add_provider_for_screen(
                    screen,
                    self.css_provider,
                    Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
                )

        self.reload_translator()

        if not self.headless and not self.settings_only:
            self.build_window()
        if not self.settings_only:
            self.connect_dbus()

        if self.app_settings is not None and not self.settings_only:
            self.app_settings.connect("changed", self.on_app_settings_changed)

        if self.settings_only:
            self.open_settings()

    # ========================================================
    # CONFIG
    # ========================================================

    def load_settings(self):

        settings = DEFAULT_SETTINGS.copy()

        try:

            with open(
                CONFIG_FILE,
                "r",
                encoding="utf-8"
            ) as file:

                saved = json.load(file)

            for key in DEFAULT_SETTINGS:

                if key in saved:
                    settings[key] = saved[key]

        except Exception:
            pass

        if self.app_settings is not None:
            try:
                if not self.app_settings.get_boolean("translation-settings-migrated"):
                    self.app_settings.set_int(
                        "translation-settle-ms",
                        int(settings.get("settle_ms", DEFAULT_SETTINGS["settle_ms"]))
                    )
                    self.app_settings.set_int(
                        "translation-min-words",
                        int(settings.get("min_words", DEFAULT_SETTINGS["min_words"]))
                    )
                    self.app_settings.set_string(
                        "translation-target-language",
                        str(settings.get("target_language", DEFAULT_TARGET_LANGUAGE))
                    )
                    self.app_settings.set_boolean("translation-settings-migrated", True)

                settings["show_dual_subtitles"] = self.app_settings.get_boolean(
                    "show-dual-subtitles"
                )
                settings["translate_on_hover"] = self.app_settings.get_boolean(
                    "translate-on-hover"
                )
                settings["settle_ms"] = self.app_settings.get_int("translation-settle-ms")
                settings["min_words"] = self.app_settings.get_int("translation-min-words")
                settings["target_language"] = self.app_settings.get_string(
                    "translation-target-language"
                )
            except Exception as error:
                print("Cannot read LiveCaptions settings:", error)

        return settings

    def on_app_settings_changed(self, settings, key):
        if key == "show-dual-subtitles":
            enabled = settings.get_boolean(key)
            self.settings["show_dual_subtitles"] = enabled
            if not self.headless:
                self.translation_label.set_visible(enabled)
                self.source_event_box.set_visible(
                    enabled or bool(self.settings.get("translate_on_hover", False))
                )
            target = self.settings.get("target_language", DEFAULT_TARGET_LANGUAGE)
            self.publish_current_line_translations(target)
            if enabled and any(self.latest_lines):
                self.last_translated_text = ""
                self.request_translation()
        elif key == "translate-on-hover":
            enabled = settings.get_boolean(key)
            self.settings["translate_on_hover"] = enabled
            if not self.headless:
                self.source_event_box.set_visible(
                    enabled or bool(self.settings.get("show_dual_subtitles", True))
                )
            if not enabled and not self.headless:
                self.clear_hover_state()
        elif key == "translation-settle-ms":
            self.settings["settle_ms"] = settings.get_int(key)
            self.save_settings()
        elif key == "translation-min-words":
            self.settings["min_words"] = settings.get_int(key)
            self.save_settings()
        elif key == "translation-packages-revision":
            self.missing_language_notice_shown = False
            if self.reload_translator():
                self.last_translated_text = ""
                if self.latest_text:
                    self.request_translation()
                if self.hover_word and self.settings.get("translate_on_hover", False):
                    self.request_hover_translation(self.hover_word)
        elif key == "translation-target-language":
            target = settings.get_string(key)
            if target == self.settings.get("target_language"):
                return
            self.settings["target_language"] = target
            self.save_settings()
            self.last_translated_text = ""
            self.missing_language_notice_shown = False
            self.clear_hover_state()
            self.publish_current_line_translations(target)
            self.reload_translator()
            if self.latest_text:
                self.request_translation()

    def publish_translation_text(self, text):
        text = text or ""
        if not self.headless:
            if self.translation_label is not None:
                self.translation_label.set_text(text)
            return

        if self.bus is None:
            return

        try:
            self.bus.call_sync(
                DBUS_SERVICE,
                "/net/sapples/LiveCaptions/Translation",
                "net.sapples.LiveCaptions.Translation",
                "SetTranslationText",
                GLib.Variant("(s)", (text,)),
                GLib.VariantType.new("()"),
                Gio.DBusCallFlags.NONE,
                2000,
                None
            )
        except GLib.Error as error:
            logging.warning("Could not update Live Captions translation: %s", error)

    def publish_hover_translation(self, text):
        text = text or ""
        if not self.headless:
            if self.source_event_box is not None:
                self.source_event_box.set_tooltip_text(text or None)
                if text:
                    self.trigger_hover_tooltip()
            return

        if self.bus is None:
            return

        try:
            self.bus.call_sync(
                DBUS_SERVICE,
                TRANSLATION_PATH,
                TRANSLATION_INTERFACE,
                "SetHoverTranslationText",
                GLib.Variant("(s)", (text,)),
                GLib.VariantType.new("()"),
                Gio.DBusCallFlags.NONE,
                2000,
                None
            )
        except GLib.Error as error:
            logging.warning("Could not update hovered word translation: %s", error)

    def save_settings(self):

        os.makedirs(
            CONFIG_DIR,
            exist_ok=True
        )

        try:

            with open(
                CONFIG_FILE,
                "w",
                encoding="utf-8"
            ) as file:

                json.dump(
                    self.settings,
                    file,
                    indent=4,
                    ensure_ascii=False
                )

        except Exception as error:

            print(
                "Cannot save settings:",
                error
            )

    def reload_translator(self):

        if ARGOS_LANGUAGE_PACKAGES.translate is None:
            with self.translation_lock:
                self.translator = None
            return False

        target_language = self.settings.get(
            "target_language",
            DEFAULT_TARGET_LANGUAGE
        )

        try:
            translator = ARGOS_LANGUAGE_PACKAGES.get_translation(target_language)
        except Exception as error:
            logging.error("Cannot load translation model: %s", error)
            translator = None

        with self.translation_lock:
            self.translator = translator

        return translator is not None

    # ========================================================
    # MAIN WINDOW
    # ========================================================

    def build_window(self):

        self.window = Gtk.Window(
            type=Gtk.WindowType.TOPLEVEL
        )

        self.window.set_title(
            "Live Translate"
        )

        # ====================================================
        # RGBA TRANSPARENCY
        # ====================================================

        screen = self.window.get_screen()

        visual = screen.get_rgba_visual()

        if visual is not None:
            self.window.set_visual(
                visual
            )

        self.window.set_app_paintable(
            True
        )

        # ====================================================
        # WINDOW BEHAVIOR
        # ====================================================

        self.window.set_decorated(
            False
        )

        self.window.set_resizable(
            False
        )

        self.window.set_keep_above(
            True
        )

        self.window.stick()

        self.window.set_skip_taskbar_hint(
            True
        )

        self.window.set_skip_pager_hint(
            True
        )

        self.window.set_type_hint(
            Gdk.WindowTypeHint.UTILITY
        )

        width = int(
            self.settings[
                "window_width"
            ]
        )

        height = int(
            self.settings[
                "window_height"
            ]
        )

        self.window.set_default_size(
            width,
            height
        )

        # Фиксируем размер.
        # Текст больше не может растягивать окно.
        self.window.set_size_request(
            width,
            height
        )

        self.window.connect(
            "delete-event",
            self.on_close
        )

        self.window.connect(
            "map-event",
            self.on_window_mapped
        )

        # ====================================================
        # ROOT
        # ====================================================

        self.root = Gtk.EventBox()

        self.root.set_name(
            "root-box"
        )

        self.root.connect(
            "button-press-event",
            self.on_root_button_press
        )

        # ====================================================
        # CONTENT
        # ====================================================

        content = Gtk.Box(
            orientation=Gtk.Orientation.HORIZONTAL,
            spacing=4
        )

        content.set_name(
            "content-box"
        )

        # ====================================================
        # CAPTION AND TRANSLATION LABELS
        # ====================================================

        text_content = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=0
        )

        self.source_event_box = Gtk.EventBox()
        self.source_event_box.set_visible_window(False)
        self.source_event_box.set_has_tooltip(True)
        self.source_event_box.add_events(
            Gdk.EventMask.POINTER_MOTION_MASK
            | Gdk.EventMask.LEAVE_NOTIFY_MASK
        )
        self.source_event_box.connect(
            "motion-notify-event",
            self.on_source_motion
        )
        self.source_event_box.connect(
            "leave-notify-event",
            self.on_source_leave
        )
        self.source_event_box.connect(
            "query-tooltip",
            self.on_query_hover_tooltip
        )

        self.source_label = Gtk.Label()
        self.source_label.set_name("source-caption")
        self.source_label.set_line_wrap(True)
        self.source_label.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR)
        self.source_label.set_lines(1)
        self.source_label.set_ellipsize(Pango.EllipsizeMode.END)
        self.source_label.set_justify(Gtk.Justification.CENTER)
        self.source_label.set_xalign(0.5)
        self.source_label.set_yalign(0.5)
        self.source_label.set_hexpand(True)
        self.source_label.set_size_request(1, 1)
        self.source_label.set_text("Waiting for LiveCaptions text stream…")

        self.source_event_box.add(self.source_label)
        self.source_event_box.set_visible(
            bool(self.settings.get("show_dual_subtitles", True)
                 or self.settings.get("translate_on_hover", False))
        )

        text_content.pack_start(
            self.source_event_box,
            True,
            True,
            0
        )

        self.translation_label = Gtk.Label()

        self.translation_label.set_name(
            "translation"
        )

        self.translation_label.set_text("Waiting for captions…")

        self.translation_label.set_line_wrap(
            True
        )

        self.translation_label.set_line_wrap_mode(
            Pango.WrapMode.WORD_CHAR
        )

        self.translation_label.set_lines(
            MAX_LINES
        )

        # Если текста слишком много —
        # не расширяем окно, а обрезаем конец.
        self.translation_label.set_ellipsize(
            Pango.EllipsizeMode.END
        )

        self.translation_label.set_justify(
            Gtk.Justification.CENTER
        )

        self.translation_label.set_xalign(
            0.5
        )

        self.translation_label.set_yalign(
            0.5
        )

        self.translation_label.set_hexpand(
            True
        )

        self.translation_label.set_vexpand(
            True
        )

        # Не разрешаем natural-size текста
        # влиять на геометрию окна.
        self.translation_label.set_size_request(
            1,
            1
        )

        text_content.pack_start(
            self.translation_label,
            True,
            True,
            0
        )

        self.translation_label.set_visible(
            bool(self.settings.get("show_dual_subtitles", True))
        )

        content.pack_start(
            text_content,
            True,
            True,
            10
        )

        # ====================================================
        # BUTTONS
        # ====================================================

        buttons = Gtk.Box(
            orientation=Gtk.Orientation.HORIZONTAL,
            spacing=2
        )

        # Settings
        self.settings_button = (
            Gtk.Button.new_with_label(
                "⚙"
            )
        )

        self.settings_button.set_name(
            "tool-button"
        )

        self.settings_button.set_relief(
            Gtk.ReliefStyle.NONE
        )

        self.settings_button.set_tooltip_text(
            "Settings"
        )

        self.settings_button.connect(
            "clicked",
            self.open_settings
        )

        # Close
        self.close_button = (
            Gtk.Button.new_with_label(
                "✕"
            )
        )

        self.close_button.set_name(
            "tool-button"
        )

        self.close_button.set_relief(
            Gtk.ReliefStyle.NONE
        )

        self.close_button.set_tooltip_text(
            "Close"
        )

        self.close_button.connect(
            "clicked",
            self.close_overlay
        )

        buttons.pack_start(
            self.settings_button,
            False,
            False,
            0
        )

        buttons.pack_start(
            self.close_button,
            False,
            False,
            0
        )

        content.pack_end(
            buttons,
            False,
            False,
            4
        )

        # ====================================================

        self.root.add(
            content
        )

        self.window.add(
            self.root
        )

        self.apply_css()

        self.window.show_all()

        self.source_event_box.set_visible(
            bool(self.settings.get("show_dual_subtitles", True)
                 or self.settings.get("translate_on_hover", False))
        )
        self.translation_label.set_visible(
            bool(self.settings.get("show_dual_subtitles", True))
        )

        # ====================================================
        # REAPPLY WINDOW RULES
        # ====================================================

        GLib.timeout_add(
            100,
            self.apply_window_rules
        )

        GLib.timeout_add(
            500,
            self.apply_window_rules
        )

        GLib.timeout_add(
            1000,
            self.apply_window_rules
        )

        GLib.idle_add(
            self.position_window_once
        )

    # ========================================================
    # WINDOW RULES
    # ========================================================

    def on_window_mapped(
        self,
        widget,
        event
    ):

        GLib.timeout_add(
            100,
            self.apply_window_rules
        )

        return False

    def apply_window_rules(self):

        # GTK/EWMH hints
        self.window.set_keep_above(
            True
        )

        self.window.stick()

        # wmctrl усиление для XWayland
        if shutil.which(
            "wmctrl"
        ):

            try:

                subprocess.run(
                    [
                        "wmctrl",
                        "-r",
                        "Live Translate",
                        "-b",
                        "add,above,sticky"
                    ],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    timeout=1
                )

            except Exception:
                pass

        return False

    # ========================================================
    # CSS
    # ========================================================

    def apply_css(self):

        opacity = float(
            self.settings[
                "background_opacity"
            ]
        )

        font_size = int(
            self.settings[
                "font_size"
            ]
        )
        source_font_size = max(9, font_size - 1)

        css = f"""
        window {{
            background-color:
                rgba(0, 0, 0, 0);
        }}

        #root-box {{
            background-color:
                rgba(
                    18,
                    18,
                    18,
                    {opacity}
                );

            border-radius: 16px;
        }}

        #content-box {{
            background-color:
                transparent;

            padding:
                4px 7px;
        }}

        #translation {{
            color:
                rgba(
                    255,
                    255,
                    255,
                    0.96
                );

            background-color:
                transparent;

            font-family:
                Ubuntu,
                Cantarell,
                Sans;

            font-size:
                {font_size}px;

            font-weight:
                500;
        }}

        #source-caption {{
            color:
                rgba(
                    255,
                    255,
                    255,
                    0.72
                );

            background-color:
                transparent;

            font-family:
                Ubuntu,
                Cantarell,
                Sans;

            font-size:
                {source_font_size}px;
        }}

        #tool-button {{
            color:
                rgba(
                    255,
                    255,
                    255,
                    0.70
                );

            background:
                transparent;

            border:
                none;

            min-width:
                24px;

            min-height:
                24px;

            padding:
                0;
        }}

        #tool-button:hover {{
            color:
                white;

            background-color:
                rgba(
                    255,
                    255,
                    255,
                    0.12
                );

            border-radius:
                12px;
        }}
        """

        # ВАЖНО:
        # обновляем существующий provider.
        # Не создаём новый каждый раз.
        self.css_provider.load_from_data(
            css.encode(
                "utf-8"
            )
        )

    # ========================================================
    # MOVE WINDOW
    # ========================================================

    def on_root_button_press(
        self,
        widget,
        event
    ):

        if (
            event.type
            == Gdk.EventType.BUTTON_PRESS

            and event.button == 1
        ):

            self.window.begin_move_drag(
                event.button,
                int(event.x_root),
                int(event.y_root),
                event.time
            )

        return False

    # ========================================================
    # POSITION
    # ========================================================

    def position_window_once(self):

        if self.positioned_once:
            return False

        self.reposition_window()

        self.positioned_once = True

        return False

    def reposition_window(self):

        screen = (
            self.window.get_screen()
        )

        monitor = (
            screen.get_primary_monitor()
        )

        if monitor < 0:
            monitor = 0

        geometry = (
            screen.get_monitor_geometry(
                monitor
            )
        )

        width = int(
            self.settings[
                "window_width"
            ]
        )

        height = int(
            self.settings[
                "window_height"
            ]
        )

        x = (
            geometry.x
            + (
                geometry.width
                - width
            ) // 2
        )

        y = (
            geometry.y
            + geometry.height
            - height
            - int(
                self.settings[
                    "bottom_margin"
                ]
            )
        )

        self.window.move(
            x,
            y
        )

    # ========================================================
    # DBUS
    # ========================================================

    def connect_dbus(self):

        self.bus = Gio.bus_get_sync(
            Gio.BusType.SESSION,
            None
        )

        self.bus.signal_subscribe(
            DBUS_SERVICE,
            DBUS_INTERFACE,
            DBUS_SIGNAL,
            DBUS_PATH,
            None,
            Gio.DBusSignalFlags.NONE,
            self.on_text_stream
        )

        self.bus.signal_subscribe(
            DBUS_SERVICE,
            TRANSLATION_INTERFACE,
            HOVER_SIGNAL,
            TRANSLATION_PATH,
            None,
            Gio.DBusSignalFlags.NONE,
            self.on_hover_word
        )

        print(
            "Listening to Live Captions "
            "D-Bus stream..."
        )

    def on_hover_word(
        self,
        connection,
        sender_name,
        object_path,
        interface_name,
        signal_name,
        parameters
    ):
        values = parameters.unpack()
        word = values[0].strip() if values else ""
        if word == self.hover_word:
            return

        self.clear_hover_state()
        if not word:
            return

        self.hover_word = word
        self.request_hover_translation(word)

    def on_text_stream(
        self,
        connection,
        sender_name,
        object_path,
        interface_name,
        signal_name,
        parameters
    ):

        values = (
            parameters.unpack()
        )

        if not values:
            return

        text = str(values[0]).replace("\r\n", "\n").replace("\r", "\n")
        lines = [line.strip() for line in text.split("\n")]
        if len(lines) < 2:
            lines.extend([""] * (2 - len(lines)))
        self.latest_lines = tuple(lines[:2])

        if (
            text
            == self.last_seen_text
        ):
            return

        self.last_seen_text = text
        self.latest_text = text
        self.pending_translation = False
        self.last_translated_text = ""

        if self.source_label is not None:
            self.source_label.set_text(text)

        # ====================================================
        # DEBOUNCE
        # ====================================================

        if (
            self.settle_id
            is not None
        ):

            try:

                GLib.source_remove(
                    self.settle_id
                )

            except Exception:
                pass

            self.settle_id = None

        # Live Captions emits an empty snapshot when the current caption ends.
        # Do not leave its last translation visible after the source disappears.
        target_language = self.settings.get(
            "target_language",
            DEFAULT_TARGET_LANGUAGE
        )
        self.publish_current_line_translations(target_language)

        if not any(self.latest_lines):
            return

        # Finished phrase:
        # translate immediately.
        if any(
            line.endswith((".", "!", "?", "…"))
            for line in self.latest_lines
            if line
        ):

            self.request_translation()

            return

        delay = int(
            self.settings[
                "settle_ms"
            ]
        )

        if delay <= 0:

            self.request_translation()

        else:

            self.settle_id = (
                GLib.timeout_add(
                    delay,
                    self.request_translation
                )
            )

    # ========================================================
    # TRANSLATION
    # ========================================================

    def publish_current_line_translations(self, target_language):
        translated_lines = []
        can_keep_previous = self.displayed_target_language == target_language

        source_lines = self.latest_lines
        if not self.settings.get("show_dual_subtitles", True):
            source_lines = ("", "")

        for index, source_line in enumerate(source_lines):
            translated = ""
            if source_line:
                translated = self.line_translation_cache.get(
                    (target_language, source_line),
                    ""
                )

                if (
                    not translated
                    and can_keep_previous
                    and index < len(self.displayed_source_lines)
                ):
                    previous_source = self.displayed_source_lines[index]
                    previous_translation = self.displayed_translation_lines[index]
                    if (
                        previous_source
                        and previous_translation
                        and source_line.startswith(previous_source)
                    ):
                        # Hold the previous result while the same caption line
                        # grows, then replace it when the updated line is ready.
                        translated = previous_translation

            translated_lines.append(translated)

        translation_text = (
            "\n".join(translated_lines)
            if any(translated_lines)
            else ""
        )
        if translation_text != self.last_published_translation:
            self.publish_translation_text(translation_text)
            self.last_published_translation = translation_text

        self.displayed_source_lines = self.latest_lines
        self.displayed_translation_lines = tuple(translated_lines)
        self.displayed_target_language = target_language
        return translation_text

    def request_translation(self):

        self.settle_id = None

        if not self.settings.get("show_dual_subtitles", True):
            return False

        lines = self.latest_lines
        if not any(lines):
            return False

        target_language = self.settings.get(
            "target_language",
            DEFAULT_TARGET_LANGUAGE
        )

        with self.translation_lock:
            translator = self.translator

        if translator is None:
            self.missing_language_notice_shown = True
            return False

        self.missing_language_notice_shown = False

        minimum_words = max(1, int(self.settings["min_words"]))
        sources = []
        for line in lines:
            if not line or len(line.split()) < minimum_words:
                continue
            if (target_language, line) in self.line_translation_cache:
                continue
            if line not in sources:
                sources.append(line)

        if not sources:
            return False

        # Не запускаем несколько Argos
        # одновременно.
        if self.translation_running:

            self.pending_translation = (
                True
            )

            return False

        self.translation_running = (
            True
        )

        self.pending_translation = (
            False
        )

        threading.Thread(
            target=self.translation_worker,
            args=(tuple(sources), target_language, translator),
            daemon=True
        ).start()

        return False

    def translation_worker(self, sources, target_language, translator):

        try:
            os.setpriority(os.PRIO_PROCESS, threading.get_native_id(), 5)
        except (AttributeError, OSError):
            pass

        results = []
        with self.translation_execution_lock:
            for source in sources:
                try:
                    result = translator.translate(source)
                    result = result.replace("\r", " ").replace("\n", " ").strip()
                except Exception as error:
                    logging.error("Translation error: %s", error)
                    result = ""
                results.append((source, result))

        GLib.idle_add(
            self.finish_translation,
            sources,
            target_language,
            results
        )

    def finish_translation(self, sources, target_language, results):

        self.translation_running = False

        for source, result in results:
            if not result:
                continue
            key = (target_language, source)
            self.line_translation_cache[key] = result
            self.line_translation_cache.move_to_end(key)
        while len(self.line_translation_cache) > 128:
            self.line_translation_cache.popitem(last=False)

        if (
            self.settings.get("target_language") == target_language
            and self.settings.get("show_dual_subtitles", True)
        ):
            self.publish_current_line_translations(target_language)

        # Only process another snapshot after its debounce timer fired.
        # on_text_stream resets this flag whenever newer caption text arrives.
        if self.pending_translation:
            self.pending_translation = False
            self.request_translation()

        return False

    # ========================================================
    # DISPLAY
    # ========================================================

    def show_translation(
        self,
        source,
        result,
        target_language
    ):

        if (
            self.latest_text.strip()
            != source
            or self.settings.get("target_language") != target_language
        ):
            return False

        self.publish_translation_text(result)

        return False

    def word_at_pointer(self, event):

        layout = self.source_label.get_layout()
        layout_x, layout_y = self.source_label.get_layout_offsets()

        inside, byte_index, trailing = layout.xy_to_index(
            int((event.x - layout_x) * Pango.SCALE),
            int((event.y - layout_y) * Pango.SCALE)
        )

        if not inside:
            return ""

        text = self.latest_text.strip()
        if not text:
            return ""

        prefix = text.encode("utf-8")[:byte_index].decode(
            "utf-8",
            errors="ignore"
        )
        character_index = min(
            len(text) - 1,
            len(prefix) + trailing
        )

        def is_word_character(character):
            return character.isalnum() or character in ("'", "’", "-")

        if not is_word_character(text[character_index]):
            return ""

        start = character_index
        end = character_index + 1

        while start > 0 and is_word_character(text[start - 1]):
            start -= 1

        while end < len(text) and is_word_character(text[end]):
            end += 1

        return text[start:end]

    def on_source_motion(self, widget, event):

        if not self.settings.get("translate_on_hover", False):
            return False

        word = self.word_at_pointer(event)

        if not word:
            self.clear_hover_state()
            return False

        if word == self.hover_word:
            return False

        self.clear_hover_state()
        self.hover_word = word

        self.hover_timeout_id = GLib.timeout_add(
            120,
            self.request_hover_translation,
            word
        )

        return False

    def on_source_leave(self, widget, event):

        self.clear_hover_state()
        return False

    def clear_hover_state(self):

        if self.hover_timeout_id is not None:
            try:
                GLib.source_remove(self.hover_timeout_id)
            except Exception:
                pass

        self.hover_timeout_id = None
        self.hover_word = ""
        self.hover_translation = ""
        self.hover_target_language = None
        self.pending_hover_word = None

        if self.headless:
            self.publish_hover_translation("")

        if getattr(self, "source_event_box", None) is not None:
            self.source_event_box.set_tooltip_text(None)

    def request_hover_translation(self, word):

        self.hover_timeout_id = None

        if (
            not self.settings.get("translate_on_hover", False)
            or word != self.hover_word
        ):
            return False

        with self.translation_lock:
            translator = self.translator

        if translator is None:
            self.hover_translation = (
                "Argos Translate is unavailable in this app runtime"
                if ARGOS_LANGUAGE_PACKAGES.import_error
                else "Download a language package in Settings"
            )
            self.publish_hover_translation(self.hover_translation)
            return False

        if self.hover_translation_running:
            self.pending_hover_word = word
            return False

        target_language = self.settings.get(
            "target_language",
            DEFAULT_TARGET_LANGUAGE
        )
        self.hover_target_language = target_language
        self.hover_translation_running = True

        threading.Thread(
            target=self.hover_translation_worker,
            args=(word, target_language, translator),
            daemon=True
        ).start()

        return False

    def hover_translation_worker(self, word, target_language, translator):

        try:
            os.setpriority(os.PRIO_PROCESS, threading.get_native_id(), 5)
        except (AttributeError, OSError):
            pass

        try:
            with self.translation_execution_lock:
                result = translator.translate(word).strip()
        except Exception as error:
            logging.error("Word translation failed: %s", error)
            result = ""

        GLib.idle_add(
            self.finish_hover_translation,
            word,
            target_language,
            result
        )

    def finish_hover_translation(self, word, target_language, result):

        self.hover_translation_running = False

        if (
            self.settings.get("translate_on_hover", False)
            and self.hover_word == word
            and self.settings.get("target_language") == target_language
        ):
            self.hover_translation = result or "No translation available"
            self.publish_hover_translation(self.hover_translation)

        pending_word = self.pending_hover_word
        self.pending_hover_word = None

        if (
            pending_word
            and pending_word == self.hover_word
            and self.settings.get("translate_on_hover", False)
        ):
            self.request_hover_translation(pending_word)

        return False

    def on_query_hover_tooltip(
        self,
        widget,
        x,
        y,
        keyboard_mode,
        tooltip
    ):

        if (
            not self.settings.get("translate_on_hover", False)
            or not self.hover_word
        ):
            return False

        if self.hover_translation:
            tooltip.set_text(self.hover_translation)
        else:
            tooltip.set_text("Translating…")

        return True

    def trigger_hover_tooltip(self):

        try:
            Gtk.Tooltip.trigger_tooltip_query(
                Gdk.Display.get_default()
            )
        except Exception:
            pass

    # ========================================================
    # SETTINGS WINDOW
    # ========================================================

    def open_settings(
        self,
        widget=None
    ):

        if (
            self.settings_window
            is not None

            and

            self.settings_window.get_visible()
        ):

            self.settings_window.present()

            return

        win = Gtk.Window(
            type=Gtk.WindowType.TOPLEVEL
        )

        self.settings_window = win

        win.set_title(
            "Live Translate — Settings"
        )

        win.set_default_size(540, 560)

        if self.window is not None:
            win.set_transient_for(self.window)

        win.set_keep_above(
            True
        )

        win.stick()

        container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        container.set_border_width(14)

        title = Gtk.Label(label="LiveCaptions translation settings")
        title.set_xalign(0)
        container.pack_start(title, False, False, 4)

        notebook = Gtk.Notebook()
        notebook.set_vexpand(True)

        translation_page = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=10
        )
        translation_page.set_border_width(12)
        self.add_language_picker(translation_page)
        self.add_slider(translation_page, "Delay", "settle_ms", 0, 300, 5, " ms")
        self.add_slider(translation_page, "Minimum words", "min_words", 1, 8, 1, "")
        notebook.append_page(translation_page, Gtk.Label(label="Translation"))

        if not self.settings_only:
            overlay_page = Gtk.Box(
                orientation=Gtk.Orientation.VERTICAL,
                spacing=10
            )
            overlay_page.set_border_width(12)
            self.add_slider(overlay_page, "Opacity", "background_opacity", 0.15, 1.0, 0.05, "", digits=2)
            self.add_slider(overlay_page, "Font size", "font_size", 10, 24, 1, " px")
            self.add_slider(overlay_page, "Window width", "window_width", 360, 1200, 10, " px")
            self.add_slider(overlay_page, "Window height", "window_height", 44, 140, 2, " px")
            self.add_slider(overlay_page, "Bottom margin", "bottom_margin", 20, 300, 5, " px")
            notebook.append_page(overlay_page, Gtk.Label(label="Overlay"))

        container.pack_start(notebook, True, True, 0)

        close_button = Gtk.Button(
            label="Done"
        )

        close_button.connect(
            "clicked",
            lambda *_:
                win.destroy()
        )

        container.pack_end(
            close_button,
            False,
            False,
            4
        )

        win.add(
            container
        )

        win.connect(
            "destroy",
            self.on_settings_destroyed
        )

        win.show_all()

        self.refresh_language_catalog()

    def on_settings_destroyed(
        self,
        widget
    ):

        self.settings_window = (
            None
        )

        self.language_combo = None
        self.language_status_label = None
        self.language_action_button = None
        self.language_refresh_button = None

        if self.settings_only:
            Gtk.main_quit()

    def add_language_picker(self, container):

        title = Gtk.Label(
            label="Translation language"
        )
        title.set_xalign(0)

        description = Gtk.Label(
            label="Live Captions recognizes English. Choose the language to translate into."
        )
        description.set_xalign(0)
        description.set_line_wrap(True)

        row = Gtk.Box(
            orientation=Gtk.Orientation.HORIZONTAL,
            spacing=6
        )

        self.language_combo = Gtk.ComboBoxText()
        self.language_combo.set_hexpand(True)
        self.language_combo.connect(
            "changed",
            self.on_language_selection_changed
        )

        self.language_action_button = Gtk.Button(
            label="Download and use"
        )
        self.language_action_button.connect(
            "clicked",
            self.on_language_action
        )

        self.language_refresh_button = Gtk.Button(
            label="Refresh"
        )
        self.language_refresh_button.connect(
            "clicked",
            self.refresh_language_catalog
        )

        row.pack_start(
            self.language_combo,
            True,
            True,
            0
        )
        row.pack_start(
            self.language_action_button,
            False,
            False,
            0
        )
        row.pack_start(
            self.language_refresh_button,
            False,
            False,
            0
        )

        self.language_status_label = Gtk.Label(
            label="Loading language packages…"
        )
        self.language_status_label.set_xalign(0)
        self.language_status_label.set_line_wrap(True)

        container.pack_start(title, False, False, 2)
        container.pack_start(description, False, False, 0)
        container.pack_start(row, False, False, 0)
        container.pack_start(
            self.language_status_label,
            False,
            False,
            0
        )

    def add_display_options(self, container):

        title = Gtk.Label(
            label="Display"
        )
        title.set_xalign(0)

        dual_subtitles_switch = Gtk.CheckButton.new_with_label(
            "Show dual subtitles"
        )
        dual_subtitles_switch.set_active(
            bool(self.settings.get("show_dual_subtitles", True))
        )
        dual_subtitles_switch.connect(
            "toggled",
            self.on_display_setting_toggled,
            "show_dual_subtitles"
        )

        hover_translation_switch = Gtk.CheckButton.new_with_label(
            "Translate word on hover"
        )
        hover_translation_switch.set_active(
            bool(self.settings.get("translate_on_hover", False))
        )
        hover_translation_switch.connect(
            "toggled",
            self.on_display_setting_toggled,
            "translate_on_hover"
        )

        container.pack_start(title, False, False, 2)
        container.pack_start(dual_subtitles_switch, False, False, 0)
        container.pack_start(hover_translation_switch, False, False, 0)

    def on_display_setting_toggled(self, switch, key):

        enabled = switch.get_active()
        self.settings[key] = enabled
        self.save_settings()

        if key == "show_dual_subtitles":
            self.translation_label.set_visible(enabled)
            self.source_event_box.set_visible(
                enabled or bool(self.settings.get("translate_on_hover", False))
            )
            if enabled and self.latest_text:
                self.last_translated_text = ""
                self.request_translation()

        elif key == "translate_on_hover":
            self.source_event_box.set_visible(
                enabled or bool(self.settings.get("show_dual_subtitles", True))
            )
            if enabled:
                self.source_label.set_text(self.latest_text)
            else:
                self.clear_hover_state()

    def refresh_language_catalog(self, widget=None):

        if self.language_catalog_loading or self.language_installing:
            return False

        self.language_catalog_loading = True

        if self.language_status_label is not None:
            self.language_status_label.set_text(
                "Loading available language packages…"
            )

        if self.language_refresh_button is not None:
            self.language_refresh_button.set_sensitive(False)

        if self.language_action_button is not None:
            self.language_action_button.set_sensitive(False)

        threading.Thread(
            target=self.load_language_catalog_worker,
            daemon=True
        ).start()

        return False

    def load_language_catalog_worker(self):
        try:
            pairs, catalog_error = ARGOS_LANGUAGE_PACKAGES.get_language_packages()
        except Exception as error:
            pairs, catalog_error = {}, str(error)

        GLib.idle_add(
            self.finish_language_catalog,
            pairs,
            catalog_error
        )

    def finish_language_catalog(self, pairs, catalog_error):

        self.language_catalog_loading = False
        self.available_language_pairs = pairs
        self.installed_target_languages = {
            code
            for code, pair_info in pairs.items()
            if pair_info.get("installed")
        }

        if self.language_combo is not None:
            self.populate_language_combo()

        if self.language_refresh_button is not None:
            self.language_refresh_button.set_sensitive(True)

        if self.language_status_label is not None:
            if catalog_error and not pairs:
                self.language_status_label.set_text(
                    "Could not load language packages: " + catalog_error
                )
            elif not pairs:
                self.language_status_label.set_text(
                    "No English translation packages were found. Check your connection and refresh."
                )
            elif catalog_error:
                self.language_status_label.set_text(
                    "Showing installed language packages. Refresh the catalog when online."
                )
            else:
                self.language_status_label.set_text(
                    "Language packages are downloaded only when you select Download and use."
                )

        self.update_language_action_state()
        return False

    def populate_language_combo(self, selected_target=None):

        if self.language_combo is None:
            return

        self.language_combo.remove_all()

        ordered_pairs = sorted(
            self.available_language_pairs.items(),
            key=lambda item: (
                (item[1].get("to_name") or item[0]).casefold(),
                item[0]
            )
        )

        for target_code, pair_info in ordered_pairs:
            language_name = pair_info.get("to_name") or target_code
            if pair_info.get("installed"):
                label = f"{language_name} ({target_code}) — installed"
            else:
                label = f"{language_name} ({target_code})"

            self.language_combo.append(target_code, label)

        target = selected_target or self.settings.get(
            "target_language",
            DEFAULT_TARGET_LANGUAGE
        )

        if target not in self.available_language_pairs:
            if DEFAULT_TARGET_LANGUAGE in self.available_language_pairs:
                target = DEFAULT_TARGET_LANGUAGE
            elif ordered_pairs:
                target = ordered_pairs[0][0]
            else:
                target = None

        if target is not None:
            self.language_combo.set_active_id(target)

        self.update_language_action_state()

    def on_language_selection_changed(self, combo):

        self.update_language_action_state()

    def update_language_action_state(self):

        if self.language_action_button is None:
            return

        target = (
            self.language_combo.get_active_id()
            if self.language_combo is not None
            else None
        )

        if target is None:
            self.language_action_button.set_label("Download and use")
            self.language_action_button.set_sensitive(False)
            return

        if target in self.installed_target_languages:
            self.language_action_button.set_label("Use language")
        else:
            self.language_action_button.set_label("Download and use")

        self.language_action_button.set_sensitive(
            not self.language_catalog_loading
            and not self.language_installing
        )

    def on_language_action(self, widget=None):

        if self.language_combo is None:
            return

        target = self.language_combo.get_active_id()
        if target is None:
            return

        if target in self.installed_target_languages:
            self.activate_target_language(target)
            return

        if target not in self.available_language_pairs:
            return

        self.language_installing = True
        self.update_language_action_state()

        if self.language_refresh_button is not None:
            self.language_refresh_button.set_sensitive(False)

        if self.language_status_label is not None:
            language_name = self.available_language_pairs[target].get(
                "to_name",
                target
            )
            self.language_status_label.set_text(
                f"Downloading English → {language_name} package…"
            )

        threading.Thread(
            target=self.install_language_worker,
            args=(target,),
            daemon=True
        ).start()

    def install_language_worker(self, target):
        error_message = None
        try:
            ARGOS_LANGUAGE_PACKAGES.install_language_package(target)
        except Exception as error:
            error_message = str(error)

        GLib.idle_add(
            self.finish_language_install,
            target,
            error_message
        )

    def finish_language_install(self, target, error_message):

        self.language_installing = False

        if error_message:
            if self.language_status_label is not None:
                self.language_status_label.set_text(
                    "Download failed: " + error_message
                )
            self.update_language_action_state()
            if self.language_refresh_button is not None:
                self.language_refresh_button.set_sensitive(True)
            return False

        self.installed_target_languages.add(target)

        if target in self.available_language_pairs:
            self.available_language_pairs[target]["installed"] = True

        self.populate_language_combo(target)
        translator_loaded = self.activate_target_language(target, update_status=False)

        # The running translation worker may have loaded before this package was
        # installed. Bump a shared setting so it reloads even when the target
        # language itself did not change.
        if self.app_settings is not None:
            try:
                revision = (self.app_settings.get_int("translation-packages-revision") + 1) & 0x7FFFFFFF
                self.app_settings.set_int("translation-packages-revision", revision)
            except Exception as error:
                logging.warning("Could not notify the running translator about installed packages: %s", error)

        if self.language_status_label is not None:
            language_name = self.available_language_pairs.get(
                target,
                {}
            ).get("to_name") or target
            if translator_loaded:
                self.language_status_label.set_text(
                    f"{language_name} is installed and ready to use offline."
                )
            else:
                self.language_status_label.set_text(
                    f"{language_name} is installed, but the translator could not load it."
                )

        if self.language_refresh_button is not None:
            self.language_refresh_button.set_sensitive(True)

        self.update_language_action_state()
        return False

    def activate_target_language(self, target, update_status=True):

        if target not in self.installed_target_languages:
            return False

        self.settings["target_language"] = target
        self.save_settings()
        if self.app_settings is not None:
            try:
                if self.app_settings.get_string("translation-target-language") != target:
                    self.app_settings.set_string("translation-target-language", target)
            except Exception as error:
                logging.warning("Could not save target language in LiveCaptions settings: %s", error)
        self.last_translated_text = ""
        self.missing_language_notice_shown = False
        self.clear_hover_state()
        self.publish_current_line_translations(target)

        if not self.reload_translator():
            if update_status and self.language_status_label is not None:
                self.language_status_label.set_text(
                    "The selected language package is installed but could not be loaded."
                )
            return False

        if update_status and self.language_status_label is not None:
            language_name = self.available_language_pairs.get(
                target,
                {}
            ).get("to_name") or target
            self.language_status_label.set_text(
                f"Using English → {language_name}. Translation works offline."
            )

        if self.latest_text:
            self.request_translation()

        return True

    # ========================================================
    # SLIDER
    # ========================================================

    def add_slider(
        self,
        container,
        title,
        key,
        minimum,
        maximum,
        step,
        suffix,
        digits=0
    ):

        wrapper = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=2
        )

        header = Gtk.Box(
            orientation=Gtk.Orientation.HORIZONTAL,
            spacing=5
        )

        label = Gtk.Label(
            label=title
        )

        label.set_xalign(
            0
        )

        value_label = Gtk.Label()

        value_label.set_xalign(
            1
        )

        header.pack_start(
            label,
            True,
            True,
            0
        )

        header.pack_end(
            value_label,
            False,
            False,
            0
        )

        adjustment = Gtk.Adjustment(
            value=self.settings[key],
            lower=minimum,
            upper=maximum,
            step_increment=step,
            page_increment=step,
            page_size=0
        )

        scale = Gtk.Scale(
            orientation=Gtk.Orientation.HORIZONTAL,
            adjustment=adjustment
        )

        scale.set_draw_value(
            False
        )

        # ====================================================

        def format_value(
            value
        ):

            if digits == 0:

                value = int(
                    round(value)
                )

            else:

                value = round(
                    value,
                    digits
                )

            return (
                f"{value}{suffix}"
            )

        value_label.set_text(
            format_value(
                self.settings[key]
            )
        )

        # ====================================================

        def changed(
            scale_widget
        ):

            value = (
                scale_widget.get_value()
            )

            if digits == 0:

                value = int(
                    round(value)
                )

            else:

                value = round(
                    value,
                    digits
                )

            self.settings[key] = (
                value
            )

            value_label.set_text(
                format_value(
                    value
                )
            )

            self.save_settings()

            # ================================================
            # LIVE CSS UPDATE
            # ================================================

            if key in (
                "background_opacity",
                "font_size"
            ):

                self.apply_css()

            # ================================================
            # LIVE WINDOW SIZE UPDATE
            # ================================================

            if key in (
                "window_width",
                "window_height"
            ):

                width = int(
                    self.settings[
                        "window_width"
                    ]
                )

                height = int(
                    self.settings[
                        "window_height"
                    ]
                )

                # Устанавливаем и min-request,
                # и текущий размер.
                self.window.set_size_request(
                    width,
                    height
                )

                self.window.resize(
                    width,
                    height
                )

            # ================================================

            if (
                key
                == "bottom_margin"
            ):

                self.reposition_window()

        # ====================================================

        scale.connect(
            "value-changed",
            changed
        )

        wrapper.pack_start(
            header,
            False,
            False,
            0
        )

        wrapper.pack_start(
            scale,
            False,
            False,
            0
        )

        container.pack_start(
            wrapper,
            False,
            False,
            3
        )

    # ========================================================
    # CLOSE
    # ========================================================

    def close_overlay(
        self,
        widget=None
    ):

        self.save_settings()

        Gtk.main_quit()

    def on_close(
        self,
        widget,
        event
    ):

        self.save_settings()

        Gtk.main_quit()

        return False


# ============================================================
# MAIN
# ============================================================

def run_language_command(args):
    def clean_field(value):
        return str(value).replace("\t", " ").replace("\r", " ").replace("\n", " ")

    if args.list_languages:
        try:
            pairs, catalog_error = ARGOS_LANGUAGE_PACKAGES.get_language_packages()
            for code, pair_info in sorted(
                pairs.items(),
                key=lambda item: (
                    (item[1].get("to_name") or item[0]).casefold(),
                    item[0]
                )
            ):
                print(
                    "LANG\t{}\t{}\t{}".format(
                        clean_field(code),
                        clean_field(pair_info.get("to_name") or code),
                        "1" if pair_info.get("installed") else "0"
                    )
                )
            if catalog_error:
                print("ERROR\t" + clean_field(catalog_error))
                return 1
            return 0
        except Exception as error:
            print("ERROR\t" + clean_field(error))
            return 1

    try:
        language_name = ARGOS_LANGUAGE_PACKAGES.install_language_package(
            args.install_language
        )
        print("OK\t" + clean_field(language_name))
        return 0
    except Exception as error:
        print("ERROR\t" + clean_field(error))
        return 1


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="LiveCaptions translation worker")
    command_group = parser.add_mutually_exclusive_group()
    command_group.add_argument(
        "--worker",
        action="store_true",
        help="run without creating the standalone translation window"
    )
    command_group.add_argument(
        "--settings",
        action="store_true",
        help="open only the translation language settings"
    )
    command_group.add_argument(
        "--list-languages",
        action="store_true",
        help="print available English translation packages for LiveCaptions"
    )
    command_group.add_argument(
        "--install-language",
        metavar="CODE",
        help="download and install one English translation package"
    )
    args = parser.parse_args()

    if args.list_languages or args.install_language:
        sys.exit(run_language_command(args))

    overlay = LiveTranslateOverlay(
        headless=args.worker,
        settings_only=args.settings
    )

    try:
        Gtk.main()

    except KeyboardInterrupt:

        pass
