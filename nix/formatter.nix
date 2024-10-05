{
  writers,
  deadnix,
  statix,
  alejandra,
  llvmPackages_19,
  fd,
  lib,
}:
writers.writeDashBin "hyprland-treewide-formatter" {
  makeWrapperArgs = [
    "--prefix"
    "PATH"
    ":"
    "${lib.makeBinPath [deadnix statix alejandra llvmPackages_19.clang-tools fd]}"
  ];
} ''
  # thanks NotAShelf for the nix formatting script :)
  nix_format() {
    if [ $@ = 0 ]; then
      fd '.*\.nix' . -E "subprojects/*\.*" -x statix fix -- {} \;
      fd '.*\.nix' . -E "subprojects/*\.*" -X deadnix -e -- {} \; -X alejandra {} \;
    elif [ -d "$1" ]; then
      fd '.*\.nix' $1 -E "subprojects/*\.*" -i -x statix fix -- {} \;
      fd '.*\.nix' $1 -E "subprojects/*\.*" -i -X deadnix -e -- {} \; -X alejandra {} \;
    else
      statix fix -- "$1"
      deadnix -e "$1"
      alejandra "$1"
    fi
  }

  cpp_format() {
    if [ $@ = 0 ] || [ "$1" = "." ]; then
      fd '.*\.cpp' . -E "subprojects/*\.*"  | xargs clang-format --verbose -i
    elif [ -d "$1" ]; then
      fd '.*\.cpp' $1 -E "subprojects/*\.*" | xargs clang-format --verbose -i
    else
      clang-format --verbose -i "$1"
    fi
  }

  for i in $@; do
    case ''${i##*.} in
      "nix")
        nix_format $i
        ;;
      "cpp")
        cpp_format $i
        ;;
      *)
        nix_format $i
        cpp_format $i
        ;;
    esac

  done
''
