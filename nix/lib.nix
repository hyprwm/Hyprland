final: prev: let
  lib = final;

  mkJoinedOverlays = overlays: final: prev:
    lib.foldl' (attrs: overlay: attrs // (overlay final prev)) {} overlays;
in prev // {
  inherit mkJoinedOverlays;
}
