# WinSpiAlertClient

Windows client for the SpiAlert Event API (C++23, MSVC, CMake).

## Requirements

- **[Visual Studio 2022 or newer](https://visualstudio.microsoft.com/downloads/)** with the *Desktop development with C++* workload (MSVC + Windows SDK)
- **[Python 3.10+](https://www.python.org/downloads/windows/)** — runs the build script
- **[Git](https://git-scm.com/downloads/win)** — CMake fetches the dependencies (rapidjson, googletest) automatically
- **[OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage/releases)** — only for `py scripts\project.py coverage`: `winget install OpenCppCoverage.OpenCppCoverage`

CMake itself is installed by the build script into a local `.venv`, nothing else is needed.

## Build

```bat
py scripts\project.py build Release
```

Result: `build\Release\client.exe`.

## Configure

Copy the example and fill in the URL and key:

```bat
copy secret\config.json.example secret\config.json
```

```json
{
	"base_url": "https://challenge.spialert.com/api/v1",
	"api_key": "chk_your_key_here"
}
```

## Run

```bat
build\Release\client.exe
```

To run against the local mock server instead of the real API (`base_url`: `https://localhost:8443/api/v1`,
`api_key`: `test-key`):

```bat
py scripts\mock_server.py install-cert   :: once, trusts the self-signed localhost certificate
py scripts\mock_server.py
```

## Tests

```bat
py scripts\project.py test          :: C++ unit tests, offline
py scripts\project.py integration   :: client against the mock server, needs mock_server.py install-cert
py scripts\project.py coverage      :: C++ line coverage of both, needs OpenCppCoverage
```
