{
  description = "Rohr Engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      packageSet = system: import nixpkgs { inherit system; };

      runtimeLibraries = pkgs: with pkgs; [
        alsa-lib
        libdecor
        libglvnd
        libx11
        libxcb
        libxcursor
        libxext
        libxfixes
        libxi
        libxkbcommon
        libxrandr
        libxscrnsaver
        libxtst
        pipewire
        pulseaudio
        wayland
      ];

      sdkPackage = system:
        let
          pkgs = packageSet system;
        in pkgs.stdenv.mkDerivation {
          pname = "rohr-sdk";
          version = "0.1.0";
          src = pkgs.lib.cleanSource ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            gnumake
            makeWrapper
            pkg-config
            wayland-scanner
          ];

          buildInputs = (runtimeLibraries pkgs) ++ (with pkgs; [
            freetype
            harfbuzz
            libffi
            wayland-protocols
          ]);

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DROHR_BUILD_EXAMPLES=OFF"
            "-DROHR_BUILD_TESTS=OFF"
            "-DROHR_ENABLE_DOCUMENTATION=OFF"
            "-DROHR_PORTABLE_SDK=ON"
          ];

          postFixup = ''
            wrapProgram "$out/bin/rohr-gui" \
              --prefix LD_LIBRARY_PATH : "${pkgs.lib.makeLibraryPath (runtimeLibraries pkgs)}"
          '';

          doInstallCheck = true;
          installCheckPhase = ''
            runHook preInstallCheck
            cmake -S "$NIX_BUILD_TOP/$sourceRoot/tests/installed_sdk_consumer" \
              -B "$TMPDIR/rohr-sdk-consumer" \
              -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_PREFIX_PATH="$out"
            cmake --build "$TMPDIR/rohr-sdk-consumer" --parallel
            runHook postInstallCheck
          '';

          meta = {
            description = "Rohr Engine SDK and editor tools";
            platforms = supportedSystems;
          };
        };

      buildExamples = system:
        let
          pkgs = packageSet system;
        in pkgs.writeShellApplication {
          name = "build-examples";

          runtimeInputs = with pkgs; [
            clang
            gnumake
            pkg-config
            coreutils
            ffmpeg
          ];

          text = ''
            if [[ ! -f Makefile ]]; then
              echo "Error: run this command from the project root."
              echo "No Makefile was found in: $PWD"
              exit 1
            fi

            echo "Clearing build/examples..."
            rm -rf build/examples
            mkdir -p build/examples

            echo "Building examples..."
            make build

            echo
            echo "Built binaries:"
            find build/examples -maxdepth 1 -type f -executable -print
          '';
        };
    in {
      packages = forAllSystems (system: {
        default = sdkPackage system;
        sdk = sdkPackage system;
      });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${buildExamples system}/bin/build-examples";
          meta.description = "Build Rohr examples";
        };
        build-examples = {
          type = "app";
          program = "${buildExamples system}/bin/build-examples";
          meta.description = "Build Rohr examples";
        };
      });

      devShells = forAllSystems (system:
        let
          pkgs = packageSet system;
        in {
          default = pkgs.mkShell {
            shellHook = ''
              export ROHR_DEV_SHELL=1
            '';

            nativeBuildInputs = with pkgs; [
              clang
              cmake
              doxygen
              pandoc
              gnumake
              pkg-config
              ffmpeg
              wayland-scanner
            ];

            buildInputs = (runtimeLibraries pkgs) ++ (with pkgs; [
              freetype
              harfbuzz
              libffi
              wayland-protocols
            ]);
          };
        });
    };
}
