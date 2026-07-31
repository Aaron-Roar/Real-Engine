{
  description = "Rohr Engine";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs {
        inherit system;
      };

      buildExamples = pkgs.writeShellApplication {
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

          export PKG_CONFIG_PATH="${
            pkgs.lib.makeSearchPath "lib/pkgconfig" [
              pkgs.sdl3
              pkgs.sdl3-image
              pkgs.sdl3-ttf
            ]
          }:${
            pkgs.lib.makeSearchPath "share/pkgconfig" [
              pkgs.sdl3
              pkgs.sdl3-image
              pkgs.sdl3-ttf
            ]
          }''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

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
      apps.${system} = {
        default = {
          type = "app";
          program = "${buildExamples}/bin/build-examples";
        };

        build-examples = {
          type = "app";
          program = "${buildExamples}/bin/build-examples";
        };
      };

      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          clang
          cmake
          doxygen
          pandoc
          gnumake
          pkg-config
          ffmpeg
        ];

        buildInputs = with pkgs; [
          sdl3
          sdl3-image
          sdl3-ttf
        ];
      };
    };
}
