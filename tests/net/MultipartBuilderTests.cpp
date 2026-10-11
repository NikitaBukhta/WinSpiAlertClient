#include <gtest/gtest.h>

#include <string>

#include "net/MultipartBuilder.h"

using net::MultipartBuilder;

TEST(MultipartBuilderTests, BuildsFilePartHeader)
{
	EXPECT_EQ(MultipartBuilder::file_part_header("BOUNDARY", "file", "a.jpg", "image/jpeg"),
	          "--BOUNDARY\r\n"
	          "Content-Disposition: form-data; name=\"file\"; filename=\"a.jpg\"\r\n"
	          "Content-Type: image/jpeg\r\n"
	          "\r\n");
}

TEST(MultipartBuilderTests, BuildsClosingBoundary)
{
	EXPECT_EQ(MultipartBuilder::closing_boundary("BOUNDARY"), "\r\n--BOUNDARY--\r\n");
}

TEST(MultipartBuilderTests, PartsFormTheSpecBody)
{
	// spec 2.3: example of a raw body
	const std::string body = MultipartBuilder::file_part_header("BOUNDARY", "file", "a.jpg", "image/jpeg") + "DATA" +
	                         MultipartBuilder::closing_boundary("BOUNDARY");

	EXPECT_EQ(body, "--BOUNDARY\r\n"
	                "Content-Disposition: form-data; name=\"file\"; filename=\"a.jpg\"\r\n"
	                "Content-Type: image/jpeg\r\n"
	                "\r\n"
	                "DATA\r\n"
	                "--BOUNDARY--\r\n");
}

TEST(MultipartBuilderTests, ContentTypeCarriesBoundary)
{
	EXPECT_EQ(MultipartBuilder::content_type("BOUNDARY"), "multipart/form-data; boundary=BOUNDARY");
}
