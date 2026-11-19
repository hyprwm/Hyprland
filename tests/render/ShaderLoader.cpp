#include <render/ShaderLoader.hpp>

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

using namespace Render;

class CShaderLoaderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_path = std::filesystem::temp_directory_path() / std::format("hyprland-shader-loader-{}", std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(m_path);

        write("main.frag", "#version 300 es\n#include \"defines.h\"\n#include \"a.glsl\"\nvoid main() {}\n");
        write("missing.frag", "#include \"nope.glsl\"\n");
        write("cyclic.frag", "#include \"cycle.glsl\"\n");
        write("defines.h", "#define FOO 0\n");
        write("a.glsl", "A\n#include \"b.glsl\"\n");
        write("b.glsl", "B\n");
        write("cycle.glsl", "#include \"cycle.glsl\"\n");

        std::array<std::string, SH_FRAG_LAST> frags;
        frags.fill("main.frag");

        m_loader = makeUnique<CShaderLoader>(std::vector<std::string>{"defines.h", "a.glsl", "b.glsl", "cycle.glsl"}, frags, m_path.string());
    }

    void TearDown() override {
        m_loader.reset();

        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    void write(const std::string& name, const std::string& content) {
        std::ofstream(m_path / name) << content;
    }

    std::filesystem::path m_path;
    UP<CShaderLoader>     m_loader;
};

TEST_F(CShaderLoaderTest, ExpandsNestedIncludesInPlace) {
    EXPECT_EQ(m_loader->process("main.frag"), "#version 300 es\n#define FOO 0\nA\nB\nvoid main() {}\n");
}

TEST_F(CShaderLoaderTest, OverridesDefines) {
    EXPECT_EQ(m_loader->process("main.frag", {{"FOO", "1"}}), "#version 300 es\n#define FOO 1\nA\nB\nvoid main() {}\n");
}

TEST_F(CShaderLoaderTest, KeepsUnresolvedIncludes) {
    EXPECT_EQ(m_loader->process("missing.frag"), "#include \"nope.glsl\"\n");
}

TEST_F(CShaderLoaderTest, StopsOnIncludeCycles) {
    EXPECT_EQ(m_loader->process("cyclic.frag"), "#include \"cycle.glsl\"\n");
}
