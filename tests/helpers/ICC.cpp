#include <helpers/cm/ColorManagement.hpp>

#include <gtest/gtest.h>

#include <filesystem>

TEST(ICC, rejectsDirectoryWithoutThrowing) {
    const auto result = NColorManagement::SImageDescription::fromICC(std::filesystem::temp_directory_path());

    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), "Failed to read file");
}
