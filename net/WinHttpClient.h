#ifndef WINSPIALERTCLIENT_WINHTTPCLIENT_H
#define WINSPIALERTCLIENT_WINHTTPCLIENT_H

#include "IHttpClient.h"

#include <expected>
#include <memory>
#include <optional>
#include <string>

#include <windows.h>
#include <winhttp.h>

namespace net
{

struct HInternetDeleter
{
	void operator()(HINTERNET handle) const noexcept
	{
		WinHttpCloseHandle(handle);
	}
};

using HInternetPtr = std::unique_ptr<void, HInternetDeleter>;

class WinHttpClient : public IHttpClient
{
public:
	WinHttpClient(const std::string& host, const std::string& user_agent,
	              INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT);
	~WinHttpClient() override;

	WinHttpClient(const WinHttpClient& other) = delete;
	WinHttpClient(WinHttpClient&& other) noexcept = delete;
	WinHttpClient& operator=(const WinHttpClient& other) = delete;
	WinHttpClient& operator=(WinHttpClient&& other) noexcept = delete;

	bool open() override;
	bool close() override;
	HttpResponse send_request(const std::string& path, const std::string& method, const std::string& body,
	                          const std::string& headers) override;

private:
	[[nodiscard]] std::expected<HInternetPtr, DWORD> open_request(const std::string& method,
	                                                              const std::string& path) const;
	static std::expected<void, DWORD> send(HINTERNET request, const std::string& headers, const std::string& body);
	static std::expected<HttpResponse, DWORD> read_response(HINTERNET request);

	static HttpResponse make_error(DWORD error_code);
	static std::optional<DWORD> query_number_header(HINTERNET request, DWORD header);
	static bool read_body(HINTERNET request, std::string& body);

private:
	std::wstring _host;
	std::wstring _user_agent;
	INTERNET_PORT _port;

	HInternetPtr _hSession;
	HInternetPtr _hConnect;
};

} // namespace net

#endif // WINSPIALERTCLIENT_WINHTTPCLIENT_H
