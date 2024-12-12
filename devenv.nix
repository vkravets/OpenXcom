{
  pkgs,
  lib,
  config,
  inputs,
  ...
}:

{
  # https://devenv.sh/basics/
  env.GREET = "devenv";

  apple.sdk = pkgs.apple-sdk_15;

  # https://devenv.sh/packages/
  packages = with pkgs; [
    xcbuild
    git
    #cmake
    clang_16
    #SDL1
    #SDL_compat
    (SDL_compat.overrideAttrs (old: {
      postInstall = ''
        ln -s $out/lib/pkgconfig/sdl12_compat.pc $out/lib/pkgconfig/sdl.pc
      '';
    }))
    #SDL_mixer
    (SDL_mixer.override (old: {
      SDL = SDL_compat;
      smpeg = old.smpeg.override {
        SDL = SDL_compat;
      };
    }))
    #SDL_image
    (
      (SDL_image.override (old: {
        SDL = SDL_compat;
      })).overrideAttrs
      (old: {
        propagatedBuildInputs = [ SDL_compat ];
        src = pkgs.fetchurl {
          url = "https://github.com/libsdl-org/SDL_image/archive/refs/heads/SDL-1.2.tar.gz";
          hash = "sha256-ytt87YL2zrvLun7bi5Jd4m5N7/UR09Y3I1kRDAqf6nQ=";
        };
        patches = [ ];
      })
    )
    #SDL_gfx
    (SDL_gfx.override (old: {
      SDL = SDL_compat;
    }))
    libwebp
    rapidyaml
    zlib
    pkg-config
  ];

  # https://devenv.sh/languages/
  #languages.rust.enable = true;

  # https://devenv.sh/processes/
  # processes.cargo-watch.exec = "cargo-watch";

  # https://devenv.sh/services/
  # services.postgres.enable = true;

  # https://devenv.sh/scripts/
  scripts.hello.exec = ''
    echo hello from $GREET
  '';

  enterShell = ''
    hello
    git --version
  '';

  # https://devenv.sh/tasks/
  # tasks = {
  #   "myproj:setup".exec = "mytool build";
  #   "devenv:enterShell".after = [ "myproj:setup" ];
  # };

}
