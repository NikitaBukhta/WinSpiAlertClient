"""Project helper: python scripts/project.py [configure|build|format|check] [Debug|Release]"""

import argparse, subprocess, sys, venv
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VENV = ROOT / ".venv"
BIN = VENV / ("Scripts" if sys.platform == "win32" else "bin")
DEPS = ["cmake", "clang-format", "cmakelang[YAML]", "ruff"]
SKIP = {".venv", ".vs", ".idea", ".git", "build", "out", "_deps"}
CPP = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}


def run(*cmd, check=True):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, check=check).returncode


def ensureVenv():
    marker = VENV / "deps.txt"
    if marker.exists() and marker.read_text() == "\n".join(DEPS):
        return
    venv.create(VENV, with_pip=True)
    run(BIN / "python", "-m", "pip", "install", "-q", "--upgrade", *DEPS)
    marker.write_text("\n".join(DEPS))


def files(match):
    for p in ROOT.rglob("*"):
        parts = p.relative_to(ROOT).parts
        if p.is_file() and match(p) and not any(d in SKIP or d.startswith("cmake-build-") for d in parts):
            yield p


def formatAll(check):
    cpp = list(files(lambda p: p.suffix.lower() in CPP))
    cmake = list(files(lambda p: p.name == "CMakeLists.txt" or p.suffix == ".cmake"))
    failed = bool(cpp) and run(
        BIN / "clang-format", "--style=file", *(["--dry-run", "--Werror"] if check else ["-i"]), *cpp, check=False
    )
    for f in cmake:
        if run(BIN / "cmake-format", "--check" if check else "-i", f, check=False):
            print(f"not formatted: {f}" if check else f"failed: {f}")
            failed = True
    py = list(files(lambda p: p.suffix == ".py"))
    if py and run(BIN / "ruff", "format", "--quiet", *(["--check"] if check else []), *py, check=False):
        failed = True
    print(f"{'Checked' if check else 'Formatted'}: {len(cpp)} C++, {len(cmake)} CMake, {len(py)} Python files")
    if failed:
        sys.exit(1)


COMMANDS = {
    "configure": "generate the build directory (cmake -S . -B build)",
    "build": "configure and build the project",
    "format": "format C++ (clang-format), CMake (cmake-format) and Python (ruff) files in place",
    "check": "check formatting without changes, exit code 1 if something is not formatted",
    "help": "show this help",
}


def parseArgs():
    parser = argparse.ArgumentParser(
        prog="python scripts/project.py",
        description="Project helper. Tools (cmake, clang-format, cmake-format, ruff) are installed into .venv on first run.",
        epilog="commands:\n" + "\n".join(f"  {k:<10} {v}" for k, v in COMMANDS.items()),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "command",
        nargs="?",
        default="build",
        choices=COMMANDS,
        metavar="command",
        help="one of: " + ", ".join(COMMANDS) + " (default: build)",
    )
    parser.add_argument(
        "config",
        nargs="?",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
        metavar="config",
        help="Debug, Release, RelWithDebInfo or MinSizeRel, used by 'build' (default: Debug)",
    )
    args = parser.parse_args()
    if args.command == "help":
        parser.print_help()
        sys.exit(0)
    return args


def main():
    args = parseArgs()
    cmd, config = args.command, args.config
    ensureVenv()
    if cmd in ("configure", "build"):
        run(BIN / "cmake", "-S", ROOT, "-B", ROOT / "build")
    if cmd == "build":
        run(BIN / "cmake", "--build", ROOT / "build", "--config", config)
    if cmd in ("format", "check"):
        formatAll(cmd == "check")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
