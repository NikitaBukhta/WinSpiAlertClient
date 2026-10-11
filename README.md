# WinSpiAlertClient

Windows client for the SpiAlert Event API (C++23, MSVC, CMake).

## Requirements

- **Visual Studio 2022 or newer** with the *Desktop development with C++* workload (MSVC + Windows SDK)
- **Python 3.10+** — runs the build script
- **Git** — CMake fetches the dependencies (rapidjson, googletest) automatically

CMake itself is installed by the build script into a local `.venv`, nothing else is needed.

## Build

```bat
py scripts\project.py build Release
```

Result: `build\Release\client.exe`. Tests: `py scripts\project.py test`.

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
