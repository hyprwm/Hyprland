{
  writers,
  deadnix,
  statix,
  alejandra,
  llvmPackages_19,
  fd,
  lib,
}:
writers.writeDashBin "fmt" {
  makeWrapperArgs = [
    "--prefix"
    "PATH"
    ":"
    "${lib.makeBinPath [deadnix statix alejandra llvmPackages_19.clang-tools fd]}"
  ];
} ''
  # thanks NotAShelf for the nix formatting script :)
  nix_format() {
    if [ -z "''${1:-""}" ] || [ "$1" = "." ]; then
      fd '.*\.nix' . -x statix fix -- {} \;
      fd '.*\.nix' . -X deadnix -e -- {} \; -X alejandra {} \;
    elif [ -d "$1" ]; then
      fd '.*\.nix' $1 -x statix fix -- {} \;
      fd '.*\.nix' $1 -X deadnix -e -- {} \; -X alejandra {} \;
    else
      statix fix -- "$1"
      deadnix -e "$1"
      alejandra "$1"
    fi
  }

  cpp_format() {
    if [ -z "''${1:-""}" ] || [ "$1" = "." ]; then
      fd '.*\.cpp' . -x clang-format --verbose -i {} \;
    elif [ -d "$1" ]; then
      fd '.*\.cpp' $1 -x clang-format --verbose -i {} \;
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