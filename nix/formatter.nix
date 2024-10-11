{
  writeShellApplication,
  deadnix,
  statix,
  alejandra,
  llvmPackages_19,
  fd,
}:
writeShellApplication {
  name = "hyprland-treewide-formatter";
  runtimeInputs = [
    deadnix
    statix
    alejandra
    llvmPackages_19.clang-tools
    fd
  ];
  text = ''
    # shellcheck disable=SC2148

    # common excludes
    excludes="subprojects"

    nix_format() {
      if [ "$*" = 0 ]; then
        fd '.*\.nix' . -E "$excludes" -x statix fix -- {} \;
        fd '.*\.nix' . -E "$excludes" -X deadnix -e -- {} \; -X alejandra {} \;
      elif [ -d "$1" ]; then
        fd '.*\.nix' "$1" -E "$excludes" -i -x statix fix -- {} \;
        fd '.*\.nix' "$1" -E "$excludes" -i -X deadnix -e -- {} \; -X alejandra {} \;
      else
        statix fix -- "$1"
        deadnix -e "$1"
        alejandra "$1"
      fi
    }

    cpp_format() {
      if [ "$*" = 0 ] || [ "$1" = "." ]; then
        fd '.*\.cpp' . -E "$excludes"  | xargs clang-format --verbose -i
      elif [ -d "$1" ]; then
        fd '.*\.cpp' "$1" -E "$excludes" | xargs clang-format --verbose -i
      else
        clang-format --verbose -i "$1"
      fi
    }

    for i in "$@"; do
      case ''${i##*.} in
        "nix")
          nix_format "$i"
          ;;
        "cpp")
          cpp_format "$i"
          ;;
        *)
          nix_format "$i"
          cpp_format "$i"
          ;;
      esac

    done
  '';
}
