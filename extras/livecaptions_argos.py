"""Argos Translate setup and offline language package operations."""

import os


class ArgosLanguagePackages:
    """Configure Argos' app-local storage and manage English language pairs."""

    def __init__(self, user_data_dir, source_language):
        self.source_language = source_language
        self.packages_dir = os.path.join(
            user_data_dir,
            "argos-translate",
            "packages",
        )
        os.environ["ARGOS_PACKAGES_DIR"] = self.packages_dir

        try:
            from argostranslate import package, translate
        except ImportError as error:
            self.package = None
            self.translate = None
            self.import_error = str(error)
        else:
            self.package = package
            self.translate = translate
            self.import_error = None

    def get_language_packages(self):
        if self.package is None:
            detail = f" ({self.import_error})" if self.import_error else ""
            raise RuntimeError(
                "Argos Translate is not installed in this app runtime" + detail
            )

        os.makedirs(self.packages_dir, exist_ok=True)
        pairs = {}
        installed_packages = self.package.get_installed_packages()
        for installed in installed_packages:
            if (
                getattr(installed, "from_code", None) != self.source_language
                or not getattr(installed, "to_code", None)
                or getattr(installed, "type", "translate") != "translate"
            ):
                continue

            target_code = installed.to_code
            pairs[target_code] = {
                "to_name": getattr(installed, "to_name", None) or target_code,
                "installed": True,
                "available": False,
            }

        try:
            self.package.update_package_index()
            available_packages = self.package.get_available_packages()
        except Exception as error:
            return pairs, str(error)

        for available in available_packages:
            if (
                getattr(available, "from_code", None) != self.source_language
                or not getattr(available, "to_code", None)
                or getattr(available, "type", "translate") != "translate"
            ):
                continue

            target_code = available.to_code
            pairs[target_code] = {
                "to_name": getattr(available, "to_name", None) or target_code,
                "installed": target_code in pairs,
                "available": True,
            }

        return pairs, None

    def install_language_package(self, target_code):
        if self.package is None:
            detail = f" ({self.import_error})" if self.import_error else ""
            raise RuntimeError(
                "Argos Translate is not installed in this app runtime" + detail
            )

        os.makedirs(self.packages_dir, exist_ok=True)
        self.package.update_package_index()
        selected_package = next(
            (
                item
                for item in self.package.get_available_packages()
                if getattr(item, "from_code", None) == self.source_language
                and getattr(item, "to_code", None) == target_code
                and getattr(item, "type", "translate") == "translate"
            ),
            None,
        )
        if selected_package is None:
            raise RuntimeError(
                "This language package is no longer available. "
                "Refresh the language list."
            )

        if hasattr(selected_package, "install"):
            selected_package.install()
        else:
            download_path = selected_package.download()
            self.package.install_from_path(download_path)
            try:
                os.remove(download_path)
            except OSError:
                pass

        return getattr(selected_package, "to_name", None) or target_code

    def get_translation(self, target_language):
        if self.translate is None:
            return None

        # A separate settings process may have installed a package since this
        # worker last looked up the available language graph.
        self.translate.get_installed_languages.cache_clear()
        return self.translate.get_translation_from_codes(
            self.source_language,
            target_language,
        )
