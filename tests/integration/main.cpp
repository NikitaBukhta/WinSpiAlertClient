#include <gtest/gtest.h>

#include "MockServerEnvironment.h"

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	testing::AddGlobalTestEnvironment(new integration::MockServerEnvironment); // gtest takes ownership
	return RUN_ALL_TESTS();
}
