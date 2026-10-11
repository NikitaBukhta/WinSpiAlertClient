#include <gtest/gtest.h>

#include "api/ApiError.h"

using api::ApiError;
using api::dto::ServerErrorDto;

TEST(ApiErrorTests, ServerErrorShowsStatusCodeAndMessage)
{
	const ApiError error =
		ApiError::server(401, "Unauthorized", ServerErrorDto{"invalid_api_key", "API key is invalid", {}});

	EXPECT_EQ(error.describe(), "401 invalid_api_key: API key is invalid");
}

TEST(ApiErrorTests, ValidationErrorShowsDetailsInsteadOfSummary)
{
	const ServerErrorDto server_error{
		"validation_error",
		"Request validation failed: event_code: Input should be ...",
		{{"event_code", "Input should be less than or equal to 10"}, {"file_name", "String should match pattern"}}};

	const ApiError error = ApiError::server(422, "Unprocessable Entity", server_error);

	EXPECT_EQ(error.describe(), "422 validation_error: event_code: Input should be less than or equal to 10; "
	                            "file_name: String should match pattern");
}

TEST(ApiErrorTests, DetailWithoutFieldShowsOnlyItsMessage)
{
	const ApiError error = ApiError::server(422, "Unprocessable Entity",
	                                        ServerErrorDto{"validation_error", "x", {{"", "Body is invalid"}}});

	EXPECT_EQ(error.describe(), "422 validation_error: Body is invalid");
}

TEST(ApiErrorTests, ServerErrorWithoutMessageShowsCode)
{
	const ApiError error = ApiError::server(500, "Internal Server Error", ServerErrorDto{"internal_error", "", {}});

	EXPECT_EQ(error.describe(), "500 internal_error");
}

TEST(ApiErrorTests, NotJsonErrorShowsStatusCodeAndText)
{
	const ApiError error = ApiError::server(502, "Bad Gateway", ServerErrorDto{});

	EXPECT_EQ(error.describe(), "502 Bad Gateway (response is not a JSON error)");
}

TEST(ApiErrorTests, NotJsonErrorWithoutStatusTextShowsOnlyStatusCode)
{
	const ApiError error = ApiError::server(502, "", ServerErrorDto{});

	EXPECT_EQ(error.describe(), "502 (response is not a JSON error)");
}

TEST(ApiErrorTests, NetworkErrorShowsWinHttpMessage)
{
	const ApiError error =
		ApiError::network(12029, "A connection with the server could not be established (error 12029)",
		                  net::FailureEnum::TRANSIENT_FAILURE);

	EXPECT_EQ(error.source, ApiError::SourceEnum::NETWORK_SOURCE);
	EXPECT_EQ(error.network_code, 12029u);
	EXPECT_EQ(error.describe(), "network error: A connection with the server could not be established (error 12029)");
}

TEST(ApiErrorTests, TransientNetworkErrorIsTransient)
{
	EXPECT_TRUE(ApiError::network(12029, "", net::FailureEnum::TRANSIENT_FAILURE).is_transient_network_error());
}

TEST(ApiErrorTests, PermanentNetworkErrorIsNotTransient)
{
	EXPECT_FALSE(ApiError::network(12175, "", net::FailureEnum::PERMANENT_FAILURE).is_transient_network_error());
}

TEST(ApiErrorTests, ServerErrorIsNotTransientNetworkError)
{
	EXPECT_FALSE(ApiError::server(503, "", ServerErrorDto{}).is_transient_network_error());
}

TEST(ApiErrorTests, LocalErrorShowsItsMessage)
{
	const ApiError error = ApiError::local("Cannot create a multipart boundary");

	EXPECT_EQ(error.source, ApiError::SourceEnum::LOCAL_SOURCE);
	EXPECT_EQ(error.describe(), "Cannot create a multipart boundary");
}
