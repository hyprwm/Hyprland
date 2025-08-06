lib: let
  inherit (lib)
    attrNames
    filterAttrs
    foldl
    generators
    partition
    ;

  inherit (lib.strings)
    concatMapStrings
    hasPrefix
    ;

  /**
    Convert a structured Nix attribute set into Hyprland's configuration format.

    This function takes a nested attribute set and converts it into Hyprland-compatible
    configuration syntax, supporting top, bottom, and regular command sections.
    
    Commands are flattened using the `flattenAttrs` function, and attributes are formatted as
    `key = value` pairs. Lists are expanded as duplicate keys to match Hyprland's expected format.

    Configuration:

    * `topCommandsPrefixes` - A list of prefixes to define **top** commands (default: `["$"]`).
    * `bottomCommandsPrefixes` - A list of prefixes to define **bottom** commands (default: `[]`).

    Attention:

    - The function ensures top commands appear **first** and bottom commands **last**.
    - The generated configuration is a **single string**, suitable for writing to a config file.
    - Lists are converted into multiple entries, ensuring compatibility with Hyprland.

    # Inputs

    Structured function argument:

    : topCommandsPrefixes (optional, default: `["$"]`)
      : A list of prefixes that define **top** commands. Any key starting with one of these
        prefixes will be placed at the beginning of the configuration.
    : bottomCommandsPrefixes (optional, default: `[]`)
      : A list of prefixes that define **bottom** commands. Any key starting with one of these
        prefixes will be placed at the end of the configuration.

    Value:

    : The attribute set to be converted to Hyprland configuration format.

    # Type

    ```
    toHyprlang :: AttrSet -> AttrSet -> String
    ```

    # Examples
    :::{.example}

    ```nix
    let
      config = {
        "$mod" = "SUPER";
        monitor = {
          "HDMI-A-1" = "1920x1080@60,0x0,1";
        };
        exec = [
          "waybar"
          "dunst"
        ];
      };
    in lib.toHyprlang {} config
    ```

    **Output:**
    ```nix
    "$mod = SUPER"
    "monitor:HDMI-A-1 = 1920x1080@60,0x0,1"
    "exec = waybar"
    "exec = dunst"
    ```

    :::
  */
  toHyprlang = {
    topCommandsPrefixes ? ["$" "bezier"],
    bottomCommandsPrefixes ? [],
  }: attrs: let
    toHyprlang' = attrs: let
      # Specially configured `toKeyValue` generator with support for duplicate keys
      # and a legible key-value separator.
      mkCommands = generators.toKeyValue {
        mkKeyValue = generators.mkKeyValueDefault {} " = ";
        listsAsDuplicateKeys = true;
        indent = ""; # No indent, since we don't have nesting
      };

      # Flatten the attrset, combining keys in a "path" like `"a:b:c" = "x"`.
      # Uses `flattenAttrs` with a colon separator.
      commands = flattenAttrs (p: k: "${p}:${k}") attrs;

      # General filtering function to check if a key starts with any prefix in a given list.
      filterCommands = list: n:
        foldl (acc: prefix: acc || hasPrefix prefix n) false list;

      # Partition keys into top commands and the rest
      result = partition (filterCommands topCommandsPrefixes) (attrNames commands);
      topCommands = filterAttrs (n: _: builtins.elem n result.right) commands;
      remainingCommands = removeAttrs commands result.right;

      # Partition remaining commands into bottom commands and regular commands
      result2 = partition (filterCommands bottomCommandsPrefixes) result.wrong;
      bottomCommands = filterAttrs (n: _: builtins.elem n result2.right) remainingCommands;
      regularCommands = removeAttrs remainingCommands result2.right;
    in
      # Concatenate strings from mapping `mkCommands` over top, regular, and bottom commands.
      concatMapStrings mkCommands [
        topCommands
        regularCommands
        bottomCommands
      ];
  in
    toHyprlang' attrs;

  /**
    Flatten a nested attribute set into a flat attribute set, using a custom key separator function.

    This function recursively traverses a nested attribute set and produces a flat attribute set
    where keys are joined using a user-defined function (`pred`). It allows transforming deeply
    nested structures into a single-level attribute set while preserving key-value relationships.

    Configuration:

    * `pred` - A function `(string -> string -> string)` defining how keys should be concatenated.
    
    # Inputs

    Structured function argument:

    : pred (required)
      : A function that determines how parent and child keys should be combined into a single key.
        It takes a `prefix` (parent key) and `key` (current key) and returns the joined key.
    
    Value:

    : The nested attribute set to be flattened.

    # Type

    ```
    flattenAttrs :: (String -> String -> String) -> AttrSet -> AttrSet
    ```

    # Examples
    :::{.example}

    ```nix
    let
      nested = {
        a = "3";
        b = { c = "4"; d = "5"; };
      };

      separator = (prefix: key: "${prefix}.${key}");  # Use dot notation
    in lib.flattenAttrs separator nested
    ```

    **Output:**
    ```nix
    {
      "a" = "3";
      "b.c" = "4";
      "b.d" = "5";
    }
    ```

    :::

  */
  flattenAttrs = pred: attrs: let
    flattenAttrs' = prefix: attrs:
      builtins.foldl' (
        acc: key: let
          value = attrs.${key};
          newKey =
            if prefix == ""
            then key
            else pred prefix key;
        in
          acc
          // (
            if builtins.isAttrs value
            then flattenAttrs' newKey value
            else {"${newKey}" = value;}
          )
      ) {} (builtins.attrNames attrs);
  in
    flattenAttrs' "" attrs;
in
{
  inherit flattenAttrs toHyprlang;
}
