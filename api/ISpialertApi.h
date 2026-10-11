#ifndef WINSPIALERTCLIENT_ISPIALERTAPI_H
#define WINSPIALERTCLIENT_ISPIALERTAPI_H

#include <optional>
#include <string>

#include "ApiResponse.h"

// TODO: move to secret/config.json ("max_image_size"), pass to SpialertApi and SpialertService
#define MAX_IMAGE_SIZE (5 * 1024 * 1024)

namespace model
{
class Image;
}

namespace api
{
namespace dto
{
struct StatusDto;
struct EventObjectDto;
struct EventRequestDto;
struct EventsListDto;
} // namespace dto

class ISpialertApi
{
public:
	virtual ~ISpialertApi() = default;

	virtual ApiResponse<dto::StatusDto> get_status() = 0;
	virtual ApiResponse<dto::EventObjectDto> post_events(const dto::EventRequestDto& event) = 0;
	virtual ApiResponse<dto::EventsListDto> get_events(int limit, int offset, std::optional<bool> image_uploaded) = 0;
	virtual ApiResponse<dto::EventObjectDto> post_image(const model::Image& image, int event_id) = 0;
};

} // namespace api

#endif // WINSPIALERTCLIENT_ISPIALERTAPI_H
