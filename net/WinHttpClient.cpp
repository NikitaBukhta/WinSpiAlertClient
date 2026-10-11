#include "WinHttpClient.h"

#include <array>

#include "NetworkConfig.h"
#include "utils/StringUtils.h"

using utils::StringUtils;

#define ERROR_TEXT_MAX_LEN 512
#define RETRY_AFTER_HEADER L"Retry-After"

namespace net
{
WinHttpClient::WinHttpClient(const std::string& host, const std::string& user_agent, INTERNET_PORT port)
	: _host(StringUtils::to_wide(host)), _user_agent(StringUtils::to_wide(user_agent)), _port(port)
{
}

WinHttpClient::~WinHttpClient() = default;

HttpResponse WinHttpClient::send_request(const std::string& path, const std::string& method,
                                         std::span<const std::string_view> body_parts, const std::string& headers)
{
	if (!_hConnect)
	{
		const auto opened = open_connection();
		if (!opened)
			return make_error(opened.error());
	}

	const auto request = open_request(method, path);
	if (!request)
		return make_error(request.error());

	const auto sent = send(request->get(), headers, body_parts);
	if (!sent)
		return make_error(sent.error());

	const auto response = read_response(request->get());
	return response ? *response : make_error(response.error());
}

std::expected<void, DWORD> WinHttpClient::open_connection()
{
	HInternetPtr session{WinHttpOpen(_user_agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
	                                 WINHTTP_NO_PROXY_BYPASS, 0)};
	if (!session)
		return std::unexpected(GetLastError());

	const BOOL timeouts_set =
		WinHttpSetTimeouts(session.get(), RESOLVE_TIMEOUT_MS, CONNECT_TIMEOUT_MS, SEND_TIMEOUT_MS, RECEIVE_TIMEOUT_MS);
	if (!timeouts_set)
		return std::unexpected(GetLastError());

	HInternetPtr connect{WinHttpConnect(session.get(), _host.c_str(), _port, 0)};
	if (!connect)
		return std::unexpected(GetLastError());

	_hSession = std::move(session);
	_hConnect = std::move(connect);
	return {};
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
                                               std::span<const std::string_view> body_parts)
{
	const std::wstring w_headers = StringUtils::to_wide(headers);
	DWORD body_size{};
	for (const std::string_view part : body_parts)
		body_size += static_cast<DWORD>(part.size());

	// the total length goes into Content-Length, the parts are written after the headers
	const BOOL sent = WinHttpSendRequest(request, w_headers.c_str(), static_cast<DWORD>(w_headers.size()),
	                                     WINHTTP_NO_REQUEST_DATA, 0, body_size, 0);
	if (!sent)
		return std::unexpected(GetLastError());

	for (const std::string_view part : body_parts)
	{
		const auto written = write_data(request, part);
		if (!written)
			return written;
	}

	const BOOL received = WinHttpReceiveResponse(request, nullptr);
	if (!received)
		return std::unexpected(GetLastError());

	return {};
}

std::expected<void, DWORD> WinHttpClient::write_data(const HINTERNET request, std::string_view data)
{
	if (data.empty())
		return {};

	DWORD written{};
	const BOOL ok = WinHttpWriteData(request, data.data(), static_cast<DWORD>(data.size()), &written);
	if (!ok)
		return std::unexpected(GetLastError());
	if (written != data.size())
		return std::unexpected(static_cast<DWORD>(ERROR_WINHTTP_CONNECTION_ERROR));

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

	response.status_code = static_cast<uint16_t>(*status_code);
	response.message = query_text_header(request, WINHTTP_QUERY_STATUS_TEXT);

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
	response.message = std::string(text.data(), length) + "(error " + std::to_string(error_code) + ")";
	response.failure = classify_error(error_code);
	return response;
}

FailureEnum WinHttpClient::classify_error(const DWORD error_code)
{
	// NOTE: an unknown code is transient. DNS failure (12007) and unreachable revocation server (12057) are
	// transient too: both happen while the network reconnects
	switch (error_code)
	{
	case ERROR_WINHTTP_INVALID_URL:
	case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:
	case ERROR_WINHTTP_SECURE_FAILURE:
	case ERROR_WINHTTP_SECURE_CHANNEL_ERROR:
	case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:
	case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:
	case ERROR_WINHTTP_SECURE_INVALID_CA:
	case ERROR_WINHTTP_SECURE_INVALID_CERT:
	case ERROR_WINHTTP_SECURE_CERT_REVOKED:
	case ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE:
	case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED:
		return FailureEnum::PERMANENT_FAILURE;
	default:
		return FailureEnum::TRANSIENT_FAILURE;
	}
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

std::string WinHttpClient::query_text_header(const HINTERNET request, const DWORD header)
{
	// NOTE: the first call only reports the size in bytes and fails with ERROR_INSUFFICIENT_BUFFER
	DWORD size{};
	WinHttpQueryHeaders(request, header, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size,
	                    WINHTTP_NO_HEADER_INDEX);
	const bool has_value = GetLastError() == ERROR_INSUFFICIENT_BUFFER && size > 0;
	if (!has_value)
		return {};

	std::wstring value(size / sizeof(wchar_t), L'\0');
	const BOOL found = WinHttpQueryHeaders(request, header, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size,
	                                       WINHTTP_NO_HEADER_INDEX);
	if (!found)
		return {};

	value.resize(size / sizeof(wchar_t));
	return StringUtils::to_utf8(value);
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
