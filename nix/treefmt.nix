_: {
  projectRootFile = "flake.nix";
  programs = {
    alejandra.enable = true;
    clang-format.enable = true;
  };
  settings.global.excludes = ["!*.cpp" "!*.nix"];
}
