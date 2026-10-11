#include <filesystem>
#include <memory>
#include <print>

#include "api/SpialertApi.h"
#include "app/SpialertService.h"
#include "model/WicImageCompressorFactory.h"
#include "net/WinHttpClient.h"
#include "utils/FileUtils.h"

// TODO: read from secret/config.json. Local mock server: scripts/mock_server.py
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8443
#define API_BASE_PATH "/api/v1"
#define API_KEY "test-key"
#define USER_AGENT "WinSpiAlertClient/1.0"

#define IMAGE_PATH_ARG 1

int wmain(int argc, wchar_t** argv)
{
	auto http_client = std::make_unique<net::WinHttpClient>(SERVER_HOST, USER_AGENT, SERVER_PORT);
	app::SpialertService spialert_service(
		std::make_unique<api::SpialertApi>(std::move(http_client), API_BASE_PATH, API_KEY),
		std::make_unique<model::WicImageCompressorFactory>());

	const bool online = spialert_service.is_online();
	std::println("Server is {}", online ? "online" : "not online");

	const bool image_given = argc > IMAGE_PATH_ARG;
	if (!image_given)
	{
		std::println("Usage: client.exe <image.png|image.jpg>");
		return online ? 0 : 1;
	}

	if (!online)
		return 1;

	const std::filesystem::path image_path = argv[IMAGE_PATH_ARG];
	if (!spialert_service.send_img(image_path))
	{
		std::println("Failed to send {}", utils::FileUtils::to_utf8(image_path));
		return 1;
	}

	std::println("Sent {}", utils::FileUtils::to_utf8(image_path));
	return 0;
}
