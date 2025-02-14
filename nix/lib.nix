lib: with lib; let
  toHyprlang = {
    topCommandsPrefixes ? ["$"],
    bottomCommandsPrefixes ? [],
  }: attrs: let
    toHyprlang' = attrs: let
      # Specially configured `toKeyValue` generator with support for duplicate
      # keys and legible key-value separator.
      mkCommands = generators.toKeyValue {
        mkKeyValue = generators.mkKeyValueDefault {} " = ";
        listsAsDuplicateKeys = true;
        # No indent, since we don't have nesting
        indent = "";
      };

      # Flatten the attrset, combining keys in a "path" like `"a:b:c" = "x"`.
      # See `flattenAttrs` for more info.
      commands = flattenAttrs (p: k: "${p}:${k}") attrs;

      # General filtering command. Used for filtering top/bottom commands.
      # Takes a list of prefixes and an element to check.
      # Returns true if any of the prefixes matched, otherwise false.
      filterCommands = list: n: foldl (acc: prefix: acc || hasPrefix prefix n) false list;

      # FIXME(docs): improve explanations for the below amalgamation

      # Get topCommands attribute names
      result = partition (filterCommands topCommandsPrefixes) (attrNames commands);
      # Filter top commands from all commands, using the attribute names
      # previously obtained
      topCommands = filterAttrs (n: _: (builtins.elem n result.right)) commands;
      # Remaining commands = allcallCommands - topCommands
      remainingCommands = removeAttrs commands result.right;

      # Get bottomCommands attr names from commands remaining (in result.wrong)
      result2 = partition (filterCommands bottomCommandsPrefixes) result.wrong;
      # Filter bottom commands from remainingCommands using the attribute names
      # previously obtained
      bottomCommands = filterAttrs (n: _: (builtins.elem n result2.right)) remainingCommands;
      # Regular commands = allCommands - topCommands - bottomCommands
      regularCommands = removeAttrs remainingCommands result2.right;
    in
      # Concatenate the strings resulting from mapping `mkCommands` over the
      # list of commands.
      concatMapStrings mkCommands [
        topCommands
        regularCommands
        bottomCommands
      ];
  in
    toHyprlang' attrs;


  /**
    Flatten a nested attribute set into a flat attribute set, joining keys with a user-defined function.

    This function takes a function `pred` that determines how nested keys should be joined,
    and an attribute set `attrs` that should be flattened.

    ## Example

    ```nix
    let
      nested = {
        a = "3";
        b = { c = "4"; d = "5"; };
      };

      separator = (prefix: key: "${prefix}.${key}");  # Use dot notation
    in flattenAttrs separator nested
    ```

    **Output:**
    ```nix
    {
      "a" = "3";
      "b.c" = "4";
      "b.d" = "5";
    }
    ```

    ## Parameters

    - **pred** : function `(string -> string -> string)`
      A function that takes a prefix and a key and returns the new flattened key.

    - **attrs** : attrset
      The nested attribute set to be flattened.

    ## Returns

    - **attrset** : A flattened attribute set where keys are joined according to `pred`.

    ## Notes

    - This function works recursively for any level of nesting.
    - It does not modify non-attribute values.
    - If `pred` is `prefix: key: "${prefix}.${key}"`, it mimics dot notation.
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
