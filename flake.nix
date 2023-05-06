{
  description = "LiveCaptions flake";
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    april-asr = {
      url = "https://april.sapples.net/aprilv0_en-us.april";
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
              wrapGAppsHook
            ];
            buildInputs = with pkgs; [
              cmake
              pkgconfig
              appstream-glib # appstream-util
              desktop-file-utils # desktop-file-utils
              gettext # msgfmt
              glib # glib-compile-schemas
              libadwaita # libadwaita-1
              libpulseaudio # libpulse
              onnxruntime
            ];
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
