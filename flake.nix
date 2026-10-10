{
  description = "LiveCaptions flake";
  inputs = {
    # april-asr is a git submodule built by meson's cmake subproject
    self.submodules = true;
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    april-asr = {
      url = "https://april.sapples.net/april-english-dev-01110_en.april";
      flake = false;
    };
  };
  outputs = {
    self,
    april-asr,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages = rec {
          livecaptions = pkgs.stdenv.mkDerivation rec {
            src = ./.;
            name = "livecaptions";
            nativeBuildInputs = with pkgs; [
              meson
              ninja
              cmake
              pkg-config
              wrapGAppsHook4
              appstream-glib # appstream-util
              desktop-file-utils # desktop-file-utils
              gettext # msgfmt
              glib # glib-compile-schemas
            ];
            buildInputs = with pkgs; [
              gtk4 # gtk4 >= 4.12
              libadwaita # libadwaita-1 >= 1.5
              libpulseaudio # libpulse
              libx11 # x11
              onnxruntime
            ];
            # cmake is only used for the april-asr meson subproject
            dontUseCmakeConfigure = true;
            preFixup = ''
              gappsWrapperArgs+=(--prefix APRIL_MODEL_PATH : "${april-asr}")
            '';
            meta = with pkgs.lib; {
              description = "Linux Desktop application that provides live captioning";
              homepage = "https://github.com/abb128/LiveCaptions";
              license = licenses.gpl3;
              platforms = platforms.linux;
            };
          };
          default = livecaptions;
        };
        apps = rec {
          livecaptions = flake-utils.lib.mkApp {drv = self.packages.${system}.livecaptions;};
          default = livecaptions;
        };
        devShells.default = pkgs.mkShell {
          inherit (self.packages.${system}.livecaptions) buildInputs nativeBuildInputs;
          shellHook = ''
            export LD_LIBRARY_PATH=${pkgs.onnxruntime}/lib
            export APRIL_MODEL_PATH=${april-asr}
          '';
        };
      }
    );
}
