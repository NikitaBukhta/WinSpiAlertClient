import argparse, hashlib, json, os, shutil, subprocess, sys, time, urllib.error, urllib.request, venv
from xml.etree import ElementTree
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VENV = ROOT / ".venv"
BIN = VENV / ("Scripts" if sys.platform == "win32" else "bin")
DEPS = ["cmake", "clang-format", "cmakelang[YAML]", "ruff", "cryptography"]
SKIP = {".venv", ".vs", ".idea", ".git", "build", "out", "_deps"}
CPP = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
COVERAGE_TOOL = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "OpenCppCoverage" / "OpenCppCoverage.exe"
COVERAGE_DIR = ROOT / "build" / "coverage"
TEST_IMAGES_MANIFEST = ROOT / "tests" / "data" / "images.json"
TEST_IMAGES_DIR = ROOT / "build" / "test-images"
DOWNLOAD_AGENT = "WinSpiAlertClient-tests/1.0"
DOWNLOAD_ATTEMPTS = 6
DOWNLOAD_RETRY_SECONDS = 30


def run(*cmd, check=True):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, check=check).returncode


def run_ctest(config, *args):
    run(BIN / "ctest", "--test-dir", ROOT / "build", "-C", config, "--output-on-failure", *args)


def ensure_venv():
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


def format_all(check):
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


def download(url):
    for attempt in range(DOWNLOAD_ATTEMPTS):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": DOWNLOAD_AGENT})
            with urllib.request.urlopen(request, timeout=600) as response:
                return response.read()
        except urllib.error.HTTPError as error:
            # Wikimedia answers 429 to several large downloads in a row
            if error.code != 429 or attempt == DOWNLOAD_ATTEMPTS - 1:
                raise
            delay = int(error.headers.get("Retry-After") or DOWNLOAD_RETRY_SECONDS)
            print(f"rate limited, retrying in {delay} s")
            time.sleep(delay)


def fetch_test_images():
    TEST_IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    for image in json.loads(TEST_IMAGES_MANIFEST.read_text(encoding="utf-8"))["images"]:
        target = TEST_IMAGES_DIR / image["name"]
        if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == image["sha256"]:
            continue
        print(f"downloading {image['name']}")
        data = download(image["url"])
        if hashlib.sha256(data).hexdigest() != image["sha256"]:
            sys.exit(f"{image['name']}: the downloaded file does not match the SHA-256 in {TEST_IMAGES_MANIFEST}")
        target.write_bytes(data)


def find_coverage_tool():
    tool = shutil.which("OpenCppCoverage") or (COVERAGE_TOOL if COVERAGE_TOOL.exists() else None)
    if not tool:
        sys.exit("OpenCppCoverage not found, install it: winget install OpenCppCoverage.OpenCppCoverage")
    return tool


def measure_coverage(config):
    tool = find_coverage_tool()
    binaries = ROOT / "build" / config
    common = ["--quiet", "--sources", ROOT, "--excluded_sources", ROOT / "build", "--excluded_sources", ROOT / "tests"]
    unit_coverage = COVERAGE_DIR / "unit.cov"
    report = COVERAGE_DIR / "coverage.xml"

    shutil.rmtree(COVERAGE_DIR, ignore_errors=True)
    run(tool, *common, "--export_type", f"binary:{unit_coverage}", "--", binaries / "client_tests.exe")
    integration_failed = run(
        tool,
        *common,
        "--input_coverage",
        unit_coverage,
        "--export_type",
        f"html:{COVERAGE_DIR / 'html'}",
        "--export_type",
        f"cobertura:{report}",
        "--",
        binaries / "client_integration_tests.exe",
        check=False,
    )
    print_coverage(report)
    print(f"HTML report: {COVERAGE_DIR / 'html' / 'index.html'}")
    if integration_failed:
        sys.exit("integration tests failed, their coverage is incomplete (is the mock certificate installed?)")


def read_coverage(report):
    # cobertura has one entry per file and per test binary: merge them into {file: {line: hit}}
    root_without_drive = str(ROOT)[len(ROOT.drive) + 1 :].lower() + "\\"
    files = {}
    for source in ElementTree.parse(report).getroot().iter("class"):
        name = source.get("filename")
        name = name[len(root_without_drive) :] if name.lower().startswith(root_without_drive) else name
        # resolve() restores the on-disk case: includes may spell a file name in another case
        name = str((ROOT / name).resolve().relative_to(ROOT.resolve()))
        lines = files.setdefault(name, {})
        for line in source.iter("line"):
            number = int(line.get("number"))
            lines[number] = lines.get(number, False) or int(line.get("hits")) > 0
    return files


def print_coverage(report):
    files = read_coverage(report)
    total_lines = total_covered = 0
    print(f"\n{'file':<34} {'lines':>9} {'coverage':>9}  not covered")
    for name, lines in sorted(files.items()):
        covered = sum(lines.values())
        missed = ", ".join(str(number) for number, hit in sorted(lines.items()) if not hit)
        print(f"{name:<34} {covered:>4}/{len(lines):<4} {100 * covered / len(lines):8.1f}%  {missed}")
        total_lines += len(lines)
        total_covered += covered
    print(f"{'total':<34} {total_covered:>4}/{total_lines:<4} {100 * total_covered / total_lines:8.1f}%")


COMMANDS = {
    "configure": "generate the build directory (cmake -S . -B build)",
    "build": "configure and build the project",
    "test": "build and run the unit tests: C++ (GoogleTest via ctest)",
    "integration": "build and run the client against the mock server (needs: mock_server.py install-cert; downloads the test images once)",
    "coverage": "build and measure C++ line coverage of unit + integration tests (needs OpenCppCoverage)",
    "format": "format C++ (clang-format), CMake (cmake-format) and Python (ruff) files in place",
    "check": "check formatting without changes, exit code 1 if something is not formatted",
    "help": "show this help",
}


def parse_args():
    parser = argparse.ArgumentParser(
        prog="python scripts/project.py",
        description="Project helper. Tools (cmake, clang-format, cmake-format, ruff) are installed into .venv on first run.",
        epilog="commands:\n" + "\n".join(f"  {k:<12} {v}" for k, v in COMMANDS.items()),
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
        help="Debug, Release, RelWithDebInfo or MinSizeRel, used by 'build', 'test', 'integration' and 'coverage' (default: Debug)",
    )
    args = parser.parse_args()
    if args.command == "help":
        parser.print_help()
        sys.exit(0)
    return args


def main():
    args = parse_args()
    cmd, config = args.command, args.config
    ensure_venv()
    if cmd in ("configure", "build", "test", "integration", "coverage"):
        run(BIN / "cmake", "-S", ROOT, "-B", ROOT / "build")
    if cmd in ("build", "test", "integration", "coverage"):
        run(BIN / "cmake", "--build", ROOT / "build", "--config", config)
    if cmd == "test":
        run_ctest(config, "--label-exclude", "integration")
    if cmd in ("integration", "coverage"):
        fetch_test_images()
    if cmd == "integration":
        run_ctest(config, "--label-regex", "integration")
    if cmd == "coverage":
        measure_coverage(config)
    if cmd in ("format", "check"):
        format_all(cmd == "check")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
