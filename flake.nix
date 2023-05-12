{
  description = "LiveCaptions flake";
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    april-asr.url = "github:nekowinston/april-asr/7baa7b6bff823e7f3672813922ae141b7b5d6d1d";
    april-asr-model = {
      url = "https://april.sapples.net/april-english-dev-01110_en.april";
      flake = false;
    };
  };
  outputs = {
    self,
    april-asr,
    april-asr-model,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            (
              final: prev: {
                aprilasr = april-asr.packages.${prev.system};
              }
            )
          ];
        };
        inherit (pkgs) lib;
        inherit (pkgs.stdenv.hostPlatform) isDarwin isLinux;
      in {
        packages = rec {
          livecaptions = pkgs.stdenv.mkDerivation {
            src = ./.;
            name = "livecaptions";
            nativeBuildInputs = with pkgs; [
              makeWrapper
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
              aprilasr.april-asr
              aprilasr.onnxruntime_1_14
            ];
            patchPhase = ''
              # remove lines containing april
              sed -i "/april/d" meson.build
              sed -i "s/april_lib/dependency('aprilasr')/" src/meson.build
            '';
            preFixup =
              lib.optionalString isLinux ''
                gappsWrapperArgs+=(--prefix APRIL_MODEL_PATH : "${april-asr-model}")
              ''
              + lib.optionalString isDarwin ''
                wrapProgram "$out/bin/livecaptions" --prefix APRIL_MODEL_PATH : "${april-asr-model}"
              '';
            meta = with lib; {
              description = "Linux Desktop application that provides live captioning";
              homepage = "https://github.com/abb128/LiveCaptions";
              license = licenses.gpl3;
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
            export APRIL_MODEL_PATH=${april-asr-model}
          '';
        };
      }
    );
}
