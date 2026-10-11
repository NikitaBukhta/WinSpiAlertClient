#include <gtest/gtest.h>

#include <regex>
#include <set>
#include <string>

#include "utils/UuidGenerator.h"

#define UUID_COUNT 1000

using utils::UuidGenerator;

TEST(UuidGeneratorTests, GeneratesCanonicalUuid)
{
	const std::regex canonical("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");

	EXPECT_TRUE(std::regex_match(UuidGenerator::generate(), canonical));
}

TEST(UuidGeneratorTests, MatchesEventUidRules)
{
	// spec: 8-64 characters from A-Z a-z 0-9 _ -
	const std::regex event_uid_rules("[A-Za-z0-9_-]{8,64}");

	EXPECT_TRUE(std::regex_match(UuidGenerator::generate(), event_uid_rules));
}

TEST(UuidGeneratorTests, GeneratesUniqueValues)
{
	std::set<std::string> uuids;
	for (int i = 0; i < UUID_COUNT; ++i)
		uuids.insert(UuidGenerator::generate());

	EXPECT_EQ(uuids.size(), static_cast<size_t>(UUID_COUNT));
}
