#include <gtest/gtest.h>

#include "api/ISpialertAPI.h"
#include "api/dto/StatusDto.h"

using api::ApiResponse;
using api::dto::StatusDto;

namespace
{

class ApiResponseSuccessTests : public testing::TestWithParam<std::pair<int, bool>>
{
};

} // namespace

TEST_P(ApiResponseSuccessTests, OnlyTwoHundredRangeIsSuccess)
{
	const auto [status_code, expected] = GetParam();
	ApiResponse<StatusDto> response{};
	response.status_code = status_code;

	EXPECT_EQ(response.is_success(), expected) << "HTTP " << status_code;
}

INSTANTIATE_TEST_SUITE_P(StatusCodes, ApiResponseSuccessTests,
                         testing::Values(std::pair{0, false}, std::pair{199, false}, std::pair{200, true},
                                         std::pair{201, true}, std::pair{299, true}, std::pair{300, false},
                                         std::pair{401, false}, std::pair{429, false}, std::pair{503, false}));
