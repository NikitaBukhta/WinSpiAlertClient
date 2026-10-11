#include <memory>
#include <print>

#include "api/SpialertApi.h"
#include "api/dto/StatusDto.h"
#include "net/WinHttpClient.h"

// TODO: read from secret/config.json. Local mock server: scripts/mock_server.py
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8443
#define API_BASE_PATH "/api/v1"
#define API_KEY "test-key"
#define USER_AGENT "WinSpiAlertClient/1.0"

int main()
{
	auto http_client = std::make_unique<net::WinHttpClient>(SERVER_HOST, USER_AGENT, SERVER_PORT);
	if (!http_client->open())
	{
		std::println("Failed to initialize the HTTP client");
		return 1;
	}

	api::SpialertApi spialert_api(std::move(http_client), API_BASE_PATH, API_KEY);

	const auto response = spialert_api.get_status();
	if (!response.is_success())
	{
		std::println("Status request failed (HTTP {})", response.status_code);
		return 1;
	}

	std::println("Server is {}", response.dto.status ? "online" : "not online");
	std::println("API version: {}", response.dto.api_version);
	std::println("Candidate: {}", response.dto.candidate);
	return 0;
}
