{
  description = "LiveCaptions flake";
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    nixpkgs-onnxruntime114.url = "github:nixos/nixpkgs/c7d48290f9da479dcab26eac5db6299739c595c2";
    # TODO: replace with upstreamed version on abb128
    april-asr.url = "github:nekowinston/april-asr/d032c6eebbff960febff50f4c56d1e1420b1b318";
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
    nixpkgs-onnxruntime114,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            # TODO: remove once https://github.com/NixOS/nixpkgs/pull/226734 is merged
            (
              final: prev: {
                onnxruntime = nixpkgs-onnxruntime114.legacyPackages.${prev.system}.onnxruntime;
                aprilasr = april-asr.packages.${prev.system}.april-asr;
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
              onnxruntime
              aprilasr
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
