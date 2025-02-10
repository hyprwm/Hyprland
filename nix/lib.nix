lib: with lib; {
  toHyprlang = {
    topCommandsPrefixes ? ["$"],
    bottomCommandsPrefixes ? [],
  }: attrs: let
    toHyprlang' = attrs: let
      toStr = x:
        if isBool x
        then boolToString x
        else toString x;

      categories = filterAttrs (n: v: isAttrs v || (isList v && all isAttrs v)) attrs;
      mkCategory = parent: attrs:
        if lib.isList attrs
        then concatMapStringsSep "\n" (a: mkCategory parent a) attrs
        else
          concatStringsSep "\n" (
            mapAttrsToList (
              k: v:
                if isAttrs v
                then mkCategory "${parent}:${k}" v
                else if isList v
                then concatMapStringsSep "\n" (item: "${parent}:${k} = ${toStr item}") v
                else "${parent}:${k} = ${toStr v}"
            )
            attrs
          );

      mkCommands = toKeyValue {
        mkKeyValue = mkKeyValueDefault {} " = ";
        listsAsDuplicateKeys = true;
        indent = "";
      };

      allCommands = filterAttrs (n: v: !(isAttrs v || (isList v && all isAttrs v))) attrs;

      filterCommands = list: n: foldl (acc: prefix: acc || hasPrefix prefix n) false list;

      # Get topCommands attr names
      result = partition (filterCommands topCommandsPrefixes) (attrNames allCommands);
      # Filter top commands from all commands
      topCommands = filterAttrs (n: _: (builtins.elem n result.right)) allCommands;
      # Remaining commands = allcallCommands - topCommands
      remainingCommands = removeAttrs allCommands result.right;

      # Get bottomCommands attr names
      result2 = partition (filterCommands bottomCommandsPrefixes) result.wrong;
      # Filter bottom commands from remainingCommands
      bottomCommands = filterAttrs (n: _: (builtins.elem n result2.right)) remainingCommands;
      # Regular commands = allCommands - topCommands - bottomCommands
      regularCommands = removeAttrs remainingCommands result2.right;
    in
      mkCommands topCommands
      + concatStringsSep "\n" (mapAttrsToList mkCategory categories)
      + mkCommands regularCommands
      + mkCommands bottomCommands;
  in
    toHyprlang' attrs;
}
