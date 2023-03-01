{
  lib,
  stdenv,
  fetchFromGitHub,
  autoreconfHook,
  python3,
}:
stdenv.mkDerivation {
  pname = "udis86";
  version = "unstable-2022-10-13";

  src = fetchFromGitHub {
    owner = "canihavesomecoffee";
    repo = "udis86";
    rev = "5336633af70f3917760a6d441ff02d93477b0c86";
    hash = "sha256-HifdUQPGsKQKQprByeIznvRLONdOXeolOsU5nkwIv3g=";
  };

  nativeBuildInputs = [autoreconfHook python3];

  configureFlags = ["--enable-shared"];

  outputs = ["bin" "out" "dev" "lib"];

  meta = with lib; {
    homepage = "https://udis86.sourceforge.net";
    license = licenses.bsd2;
    mainProgram = "udcli";
    description = "Easy-to-use, minimalistic x86 disassembler library (libudis86)";
    platforms = platforms.all;
  };
}
