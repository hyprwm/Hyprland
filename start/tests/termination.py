import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile


def main():
    launcher, case = sys.argv[1:]
    if case.startswith("exit-"):
        code = int(case[len("exit-"):])
        termination = f"sys.exit({code})"
        expected = f"Hyprland exited with code {code}"
    else:
        sig = {"sigterm": signal.SIGTERM, "sigkill": signal.SIGKILL}[case]
        termination = f"os.kill(os.getpid(), {int(sig)})"
        expected = f"Hyprland terminated by signal {int(sig)}"

    with tempfile.TemporaryDirectory(prefix="start-hyprland-test-") as directory:
        helper = Path(directory) / "child"
        helper.write_text(f"#!{sys.executable}\nimport os\nimport sys\n{termination}\n")
        helper.chmod(0o700)
        with subprocess.Popen(
            [launcher, "--no-nixgl", "--path", str(helper)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            start_new_session=True,
        ) as process:
            try:
                output, _ = process.communicate(timeout=10)
            finally:
                # Also clean up descendants if the launcher fails or hangs.
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait(timeout=5)

    assert process.returncode == 0, output
    assert output.count(expected) == 1, output
    assert "Couldn't waitpid" not in output, output
    assert "ECHILD" not in output, output
    assert "Hyprland exit cleanly." in output, output
    assert "restarting" not in output, output


if __name__ == "__main__":
    main()
