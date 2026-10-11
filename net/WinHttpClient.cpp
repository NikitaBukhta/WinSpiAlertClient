#include "WinHttpClient.h"

#include <array>

#include "utils/StringUtils.h"

using utils::StringUtils;

#define RESOLVE_TIMEOUT_MS 10'000
#define CONNECT_TIMEOUT_MS 10'000
#define SEND_TIMEOUT_MS 30'000
#define RECEIVE_TIMEOUT_MS 30'000
#define READ_CHUNK_SIZE (8 * 1024)
#define ERROR_TEXT_MAX_LEN 512
#define RETRY_AFTER_HEADER L"Retry-After"

namespace net
{
WinHttpClient::WinHttpClient(const std::string& host, const std::string& user_agent, INTERNET_PORT port)
	: _host(StringUtils::to_wide(host)), _user_agent(StringUtils::to_wide(user_agent)), _port(port)
{
}

WinHttpClient::~WinHttpClient() = default;

bool WinHttpClient::open()
{
	close();

	HInternetPtr session{WinHttpOpen(_user_agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
	                                 WINHTTP_NO_PROXY_BYPASS, 0)};
	if (!session)
		return false;

	const BOOL timeouts_set =
		WinHttpSetTimeouts(session.get(), RESOLVE_TIMEOUT_MS, CONNECT_TIMEOUT_MS, SEND_TIMEOUT_MS, RECEIVE_TIMEOUT_MS);
	if (!timeouts_set)
		return false;

	HInternetPtr connect{WinHttpConnect(session.get(), _host.c_str(), _port, 0)};
	if (!connect)
		return false;

	_hSession = std::move(session);
	_hConnect = std::move(connect);
	return true;
}

bool WinHttpClient::close()
{
	_hConnect.reset();
	_hSession.reset();
	return true;
}

HttpResponse WinHttpClient::send_request(const std::string& path, const std::string& method, const std::string& body,
                                         const std::string& headers)
{
	if (!_hConnect)
		return make_error(ERROR_WINHTTP_INCORRECT_HANDLE_STATE);

	const auto request = open_request(method, path);
	if (!request)
		return make_error(request.error());

	const auto sent = send(request->get(), headers, body);
	if (!sent)
		return make_error(sent.error());

	const auto response = read_response(request->get());
	return response ? *response : make_error(response.error());
}

std::expected<HInternetPtr, DWORD> WinHttpClient::open_request(const std::string& method, const std::string& path) const
{
	const std::wstring w_method = StringUtils::to_wide(method);
	const std::wstring w_path = StringUtils::to_wide(path);

	// NOTE: HTTPS only
	HInternetPtr request{WinHttpOpenRequest(_hConnect.get(), w_method.c_str(), w_path.c_str(), nullptr,
	                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
	if (!request)
		return std::unexpected(GetLastError());

	// check the server certificate is not revoked (off by default in WinHTTP)
	DWORD features = WINHTTP_ENABLE_SSL_REVOCATION;
	if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_ENABLE_FEATURE, &features, sizeof(features)))
		return std::unexpected(GetLastError());

	return request;
}

std::expected<void, DWORD> WinHttpClient::send(const HINTERNET request, const std::string& headers,
                                               const std::string& body)
{
	const std::wstring w_headers = StringUtils::to_wide(headers);
	const auto body_size = static_cast<DWORD>(body.size());

	// NOTE: WinHttpSendRequest takes a non-const pointer, but does not modify the body
	const BOOL sent = WinHttpSendRequest(request, w_headers.c_str(), static_cast<DWORD>(w_headers.size()),
	                                     const_cast<char*>(body.data()), body_size, body_size, 0);
	if (!sent)
		return std::unexpected(GetLastError());

	const BOOL received = WinHttpReceiveResponse(request, nullptr);
	if (!received)
		return std::unexpected(GetLastError());

	return {};
}

std::expected<HttpResponse, DWORD> WinHttpClient::read_response(const HINTERNET request)
{
	HttpResponse response;
	const auto status_code = query_number_header(request, WINHTTP_QUERY_STATUS_CODE);
	if (!status_code || !read_body(request, response.body))
		return std::unexpected(GetLastError());

	// NOTE: when the server closes the connection mid-body, WinHttpReadData just reports the end of data
	const auto content_length = query_number_header(request, WINHTTP_QUERY_CONTENT_LENGTH);
	const bool body_complete = !content_length || response.body.size() == *content_length;
	if (!body_complete)
		return std::unexpected(static_cast<DWORD>(ERROR_WINHTTP_CONNECTION_ERROR));

	response.status_code = *status_code;

	// NOTE: WINHTTP_QUERY_RETRY_AFTER does not support WINHTTP_QUERY_FLAG_NUMBER (fails with 12154), query it by name
	if (const auto retry_after = query_number_header(request, WINHTTP_QUERY_CUSTOM, RETRY_AFTER_HEADER))
		response.retry_after = std::chrono::seconds{*retry_after};

	return response;
}

HttpResponse WinHttpClient::make_error(const DWORD error_code)
{
	// NOTE: English text keeps the message ASCII; if English resources are missing, only the code is reported
	constexpr DWORD flags = FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
	                        FORMAT_MESSAGE_MAX_WIDTH_MASK;
	std::array<char, ERROR_TEXT_MAX_LEN> text{};
	const DWORD length = FormatMessageA(flags, GetModuleHandleW(L"winhttp.dll"), error_code,
	                                    MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), text.data(),
	                                    static_cast<DWORD>(text.size()), nullptr);

	HttpResponse response;
	response.error_code = error_code;
	response.error_message = std::string(text.data(), length) + "(error " + std::to_string(error_code) + ")";
	return response;
}

std::optional<DWORD> WinHttpClient::query_number_header(const HINTERNET request, const DWORD header,
                                                        const wchar_t* name)
{
	DWORD value{};
	DWORD size = sizeof(value);
	const BOOL found =
		WinHttpQueryHeaders(request, header | WINHTTP_QUERY_FLAG_NUMBER, name, &value, &size, WINHTTP_NO_HEADER_INDEX);
	if (!found)
		return std::nullopt;

	return value;
}

bool WinHttpClient::read_body(const HINTERNET request, std::string& body)
{
	std::array<char, READ_CHUNK_SIZE> chunk{};
	DWORD bytes_read{};

	do
	{
		if (!WinHttpReadData(request, chunk.data(), READ_CHUNK_SIZE, &bytes_read))
			return false;

		body.append(chunk.data(), bytes_read);
	} while (bytes_read > 0);

	return true;
}
} // namespace net
