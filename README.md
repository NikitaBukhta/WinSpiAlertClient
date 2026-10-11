# WinSpiAlertClient

Windows client for the SpiAlert Event API (C++23, MSVC, CMake).

## Requirements

- **[Visual Studio 2026](https://visualstudio.microsoft.com/downloads/)** with the *Desktop development with C++* workload (MSVC + Windows SDK).
  Built and tested with Visual Studio Community 2026 18.10 (MSVC 19.51, Windows SDK 10.0.26100). The code uses C++23
  (`std::print`, `std::expected`, `append_range`), so Visual Studio 2022 is not checked and may lack some of it.
- **[Python 3.10+](https://www.python.org/downloads/windows/)** — runs the build script
- **[Git](https://git-scm.com/downloads/win)** — CMake fetches the dependencies automatically: rapidjson at a fixed
  commit and googletest 1.17.0
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

Run from the repository root: the default config path `secret/config.json` is relative to the current directory.

```bat
build\Release\client.exe status                 :: is the server online
build\Release\client.exe send capture.png       :: create an event and upload the image (PNG or JPEG)
build\Release\client.exe upload 42 capture.png  :: upload the image into existing event 42, e.g. after a failed send
build\Release\client.exe list [--pending]       :: list events, --pending: only events without an image
build\Release\client.exe help
```

Options, accepted anywhere on the line:

- `--config <path>`: another config file instead of `secret/config.json`.
- `--url <url>`, `--key <key>`: used instead of `base_url` and `api_key` from the file. With both given, the
  default config file is not needed. Prefer the file for the key: a `--key` stays in the shell history and the process list.
- `--`: ends the options, for an image whose name starts with `-`: `send -- -capture.png`.

Only `https` URLs are accepted. The exit code is 0 on success and 1 on error, a call without a command shows the help
and returns 1.

To run against the local mock server instead of the real API:

```bat
py scripts\mock_server.py install-cert   :: once, trusts the self-signed localhost certificate
py scripts\mock_server.py
build\Release\client.exe --url https://localhost:8443/api/v1 --key test-key status
```

## Tests

```bat
py scripts\project.py test          :: C++ unit tests, offline
py scripts\project.py integration   :: client against the mock server, needs mock_server.py install-cert
py scripts\project.py coverage      :: C++ line coverage of both, needs OpenCppCoverage
```

## Rate limit and retries

Every request to the server gets at most 3 attempts (`app/SpialertService::send_with_retry`):

- 429 Too Many Requests: the client prints `Rate limited, retrying in N s` and waits `Retry-After` seconds, 5 s
  without the header. The server has not processed a 429 request, so the resend is safe; an event is resent with the
  same `event_uid`.
- A network error: the client prints `Network error, retrying in N s` and waits 1 s, then 2 s. A lost response may
  still have reached the server: the same `event_uid` returns the existing event, and 409 on the image upload means
  the lost attempt has already uploaded it.
- A rejected certificate or another TLS error is not retried: a resend fails the same way.
- A 2xx response with a body that cannot be read (not JSON, an event without `event_id`) is reported as an error,
  not taken for success.

Before `send` the client checks `/status`. If the server is not online, it prints `Server is not online, retrying in
N s` and asks again after 1 s and 2 s, then gives up.

## Large images

An image over 5 MB is shrunk before the upload. It keeps its format and name, only the quality and the resolution
change (`model/WicImageCompressor`, `model/ImageSizePredictor`):

1. A JPEG at most 1.5x over the limit is re-encoded at quality 0.75.
2. Then the resolution goes down at quality 0.75, each step estimated from the previous size.
3. The last try is quality 0.6 with the shorter side at 640 px. If it still does not fit, the image is rejected.

There are at most 4 encodes. The EXIF orientation is applied to the pixels and the rest of the metadata is dropped.
Images over 400 megapixels are rejected, images over 24 megapixels are shrunk while being decoded.

The integration tests also send real photos from Wikimedia Commons (CC0 and public domain, listed in
`tests/data/images.json`). `py scripts\project.py integration` downloads them into `build\test-images` on the first run.

## Known limitations

- A request gets at most 3 attempts, so a server that answers 429 three times in a row ends the command.
- `Retry-After` is read only as a number of seconds. The HTTP-date form falls back to the 5 s default.
- A 5xx answer is not retried, only network errors and 429 are: a 503 from a proxy while the server restarts ends
  the command at once.
- `list` reads at most 20 pages of 100 events (2000 events) and says so when the list is cut.
- `list --pending` pages by offset over a filter. If another client uploads images while the pages are read, the
  filtered list shifts and some events may be skipped.
- The default config path `secret/config.json` is relative to the current directory, not to `client.exe`.
- The certificate revocation check needs the OCSP or CRL server. If it cannot be reached (error 12057), the client
  retries it as a network error and then fails: the check is never turned off.
- File names must follow the server rule: 1–100 characters from `A-Z a-z 0-9 _ -` plus `.png`, `.jpg` or `.jpeg`.
- Only Visual Studio 2026 is checked, see Requirements.

## Possible improvements

### Client

- Encrypt `api_key` in the config with DPAPI (`CryptProtectData`): only the same Windows user can decrypt it, so a
  copied config file or a backup does not leak the key. A `config protect` command would encrypt it in place.
- `upload-pending`: upload all the images for events left without one at once, instead of one `upload` per event.
  The client would keep a local journal of `event_id` and image path, since the server stores only the file name.
- Log every request to a file: method, path, status, attempt, duration, never the key. The server counts failed
  attempts, so the log shows what the client did and why.
- A client side rate limiter (60 requests per minute) to avoid 429 at all when sending many images or long lists.
- Send several images or a folder in one run.
- Skip the `/status` check before `send`: it costs one of three requests per image, and a server error is reported
  anyway.
- `list --json` for scripts.

### Compression

We don't know yet what images the client will get, so the compression is kept simple. Things to add once we do:

- Search the JPEG quality between 0.75 and 0.95 instead of always using 0.75.
- Make one more, larger attempt when a result leaves much of the 5 MB unused.
- Keep the RGB color profile, so wide gamut photos (Display P3, Adobe RGB) do not look washed out.
- Convert CMYK JPEG through its color profile (`IWICColorTransform`) instead of the plain converter.
- Keep gray images gray and opaque PNG without alpha, which makes the files smaller.
- Skip the useless re-encode of a PNG whose shorter side is already 640 px or less.
- Check the thresholds (0.75, 0.6, 640 px, 4 encodes, 24 and 400 megapixels) on real captures and adjust them.
