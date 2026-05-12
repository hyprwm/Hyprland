#!/usr/bin/env bash
#
# Adapted from https://github.com/jidicula/clang-format-action

###############################################################################
#                                check.sh                                     #
###############################################################################
# USAGE: ./entrypoint.sh [<path>] [<fallback style>]
#
# Checks all C/C++/Protobuf/CUDA files (.h, .H, .hpp, .hh, .h++, .hxx and .c,
# .C, .cpp, .cc, .c++, .cxx, .proto, .cu) in the provided GitHub repository path
# (arg1) for conforming to clang-format. If no path is provided or provided path
# is not a directory, all C/C++/Protobuf/CUDA files are checked. If any files
# are incorrectly formatted, the script lists them and exits with 1.
#
# Define your own formatting rules in a .clang-format file at your repository
# root. Otherwise, the provided style guide (arg2) is used as a fallback.

# format_diff function
# Accepts a filepath argument. The filepath passed to this function must point
# to a C/C++/Protobuf/CUDA file.
format_diff() {
  local filepath="$1"

  # Invoke clang-format with dry run and formatting error output
  local_format="$(clang-format \
    --dry-run \
    --Werror \
    --style=file \
    --fallback-style="$FALLBACK_STYLE" \
    "${filepath}")"

  local format_status="$?"
  if [[ ${format_status} -ne 0 ]]; then
    # Append Markdown-bulleted monospaced filepath of failing file to
    # summary file.
    echo "* \`$filepath\`" >>failing-files.txt

    echo "Failed on file: $filepath" >&2
    echo "$local_format" >&2
    exit_code=1 # flip the global exit code
    return "${format_status}"
  fi
  return 0
}

CHECK_PATH="$1"
FALLBACK_STYLE="$2"
EXCLUDE_REGEX="$3"
INCLUDE_REGEX="$4"

# Set the regex to an empty string regex if nothing was provided
if [[ -z $EXCLUDE_REGEX ]]; then
  EXCLUDE_REGEX="^$"
fi

# Set the filetype regex if nothing was provided.
# Find all C/C++/Protobuf/CUDA files:
#   h, H, hpp, hh, h++, hxx
#   c, C, cpp, cc, c++, cxx
#   ino, pde
#   proto
#   cu
if [[ -z $INCLUDE_REGEX ]]; then
  INCLUDE_REGEX='^.*\.((((c|C)(c|pp|xx|\+\+)?$)|((h|H)h?(pp|xx|\+\+)?$))|(ino|pde|proto|cu))$'
fi

cd "$GITHUB_WORKSPACE" || exit 2

if [[ ! -d $CHECK_PATH ]]; then
  echo "Not a directory in the workspace, fallback to all files." >&2
  CHECK_PATH="."
fi

# initialize exit code
exit_code=0

# All files improperly formatted will be printed to the output.
src_files=$(find "$CHECK_PATH" -name .git -prune -o -regextype posix-egrep -regex "$INCLUDE_REGEX" -print)

# check formatting in each source file
IFS=$'\n' # Loop below should separate on new lines, not spaces.
for file in $src_files; do
  # Only check formatting if the path doesn't match the regex
  if ! [[ ${file} =~ $EXCLUDE_REGEX ]]; then
    format_diff "${file}"
  fi
done

# global exit code is flipped to nonzero if any invocation of `format_diff` has
# a formatting difference.
exit "$exit_code"
