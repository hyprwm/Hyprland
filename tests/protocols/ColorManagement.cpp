#include <protocols/ColorManagement.hpp>
#include <Compositor.hpp>

#include <gtest/gtest.h>
#include <hyprutils/os/FileDescriptor.hpp>

#include <array>
#include <cerrno>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <sys/socket.h>

using namespace Hyprutils::OS;
using namespace NColorManagement;

// Exercise real requests on a private display and socketpair, without starting
// a compositor session. Version 1 supplies the description identity in ready.
class CColorManagementTest : public ::testing::Test {
  protected:
    void SetUp() override;
    void TearDown() override;
    void sendRequest(std::initializer_list<uint32_t> request);
    void readExact(std::span<std::byte> bytes);
    void readDescription(uint32_t id, PImageDescription& description);
    void createParametric(wpColorManagerV1TransferFunction tf, PImageDescription& description, std::optional<std::array<uint32_t, 3>> luminances = std::nullopt,
                          bool luminancesFirst = false);
    void expectLuminances(PImageDescription description, float min, uint32_t max, uint32_t reference);

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    wl_client*                                                 m_client           = nullptr; // Owned by m_display.
    uint32_t                                                   m_nextID           = 3;
    bool                                                       m_installedGlobals = false;
    UP<CCompositor>                                            m_previousCompositor;
    UP<CColorManagementProtocol>                               m_previousColorManagement;
};

void CColorManagementTest::SetUp() {
    m_display.reset(wl_display_create());
    ASSERT_NE(m_display, nullptr);

    std::array<int, 2> sockets = {
        -1,
        -1,
    };
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor serverSocket{sockets[0]};
    m_socket = CFileDescriptor{sockets[1]};
    m_client = wl_client_create(m_display.get(), serverSocket.get());
    ASSERT_NE(m_client, nullptr);
    serverSocket.take();

    m_previousCompositor      = std::move(g_pCompositor);
    m_previousColorManagement = std::move(PROTO::colorManagement);
    m_installedGlobals        = true;

    g_pCompositor              = makeUnique<CCompositor>(true);
    g_pCompositor->m_wlDisplay = m_display.get();
    PROTO::colorManagement     = makeUnique<CColorManagementProtocol>(&wp_color_manager_v1_interface, 1, "color-management-test");
    ASSERT_NE(PROTO::colorManagement->getGlobal(), nullptr);
    PROTO::colorManagement->bindManager(m_client, nullptr, 1, 2);
    ASSERT_NE(wl_client_get_object(m_client, 2), nullptr);
}

void CColorManagementTest::TearDown() {
    // Resource destruction callbacks must still see the test protocol.
    if (m_display)
        wl_display_destroy_clients(m_display.get());
    m_client = nullptr;
    m_socket.reset();

    if (m_installedGlobals) {
        PROTO::colorManagement.reset();
        g_pCompositor.reset();
    }
    m_display.reset();

    if (m_installedGlobals) {
        g_pCompositor          = std::move(m_previousCompositor);
        PROTO::colorManagement = std::move(m_previousColorManagement);
    }
}

void CColorManagementTest::sendRequest(std::initializer_list<uint32_t> request) {
    const auto bytes = request.size() * sizeof(uint32_t);
    ASSERT_EQ(send(m_socket.get(), request.begin(), bytes, MSG_NOSIGNAL | MSG_DONTWAIT), sc<ssize_t>(bytes));
    ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
}

void CColorManagementTest::readExact(std::span<std::byte> bytes) {
    while (!bytes.empty()) {
        const auto received = recv(m_socket.get(), bytes.data(), bytes.size(), MSG_DONTWAIT);
        ASSERT_GT(received, 0) << "Missing Wayland event bytes, errno=" << errno;
        bytes = bytes.subspan(sc<size_t>(received));
    }
}

void CColorManagementTest::readDescription(uint32_t id, PImageDescription& description) {
    wl_display_flush_clients(m_display.get());

    // Manager advertisements and wl_display.delete_id can precede ready.
    // Every expected event has either no payload or one uint32_t.
    for (size_t event = 0; event < 64; ++event) {
        std::array<uint32_t, 2> header = {};
        ASSERT_NO_FATAL_FAILURE(readExact(std::as_writable_bytes(std::span{header})));
        const auto size   = header[1] >> 16;
        const auto opcode = header[1] & 0xFFFFU;
        ASSERT_FALSE(header[0] == 1 && opcode == 0) << "Wayland protocol error";
        ASSERT_GE(size, sizeof(header));
        ASSERT_LE(size, sizeof(header) + sizeof(uint32_t));
        ASSERT_EQ(size % sizeof(uint32_t), 0U);

        std::array<uint32_t, 1> payload = {};
        ASSERT_NO_FATAL_FAILURE(readExact(std::as_writable_bytes(std::span{payload}).first(size - sizeof(header))));
        if (header[0] != id)
            continue;

        ASSERT_EQ(opcode, 1U) << "Expected image description ready";
        ASSERT_EQ(size, sizeof(header) + sizeof(uint32_t));
        ASSERT_NE(payload[0], 0U);
        description = CImageDescription::from(sc<uint64_t>(payload[0]));
        ASSERT_TRUE(description);
        return;
    }

    FAIL() << "No ready event for image description " << id;
}

void CColorManagementTest::createParametric(wpColorManagerV1TransferFunction tf, PImageDescription& description, std::optional<std::array<uint32_t, 3>> luminances,
                                            bool luminancesFirst) {
    const auto creatorID     = m_nextID++;
    const auto descriptionID = m_nextID++;
    // Manager opcode 5 creates params; params opcodes 3/1/5 set primaries/TF/luminances.
    ASSERT_NO_FATAL_FAILURE(sendRequest({
        2,
        (12U << 16) | 5U,
        creatorID,
    }));
    ASSERT_NO_FATAL_FAILURE(sendRequest({
        creatorID,
        (12U << 16) | 3U,
        WP_COLOR_MANAGER_V1_PRIMARIES_SRGB,
    }));
    if (luminances && luminancesFirst) {
        ASSERT_NO_FATAL_FAILURE(sendRequest({
            creatorID,
            (20U << 16) | 5U,
            (*luminances)[0],
            (*luminances)[1],
            (*luminances)[2],
        }));
    }
    ASSERT_NO_FATAL_FAILURE(sendRequest({
        creatorID,
        (12U << 16) | 1U,
        tf,
    }));
    if (luminances && !luminancesFirst) {
        ASSERT_NO_FATAL_FAILURE(sendRequest({
            creatorID,
            (20U << 16) | 5U,
            (*luminances)[0],
            (*luminances)[1],
            (*luminances)[2],
        }));
    }
    // Params opcode 0 consumes the creator and creates the image description.
    ASSERT_NO_FATAL_FAILURE(sendRequest({
        creatorID,
        12U << 16,
        descriptionID,
    }));
    ASSERT_NO_FATAL_FAILURE(readDescription(descriptionID, description));
    EXPECT_EQ(description->value().transferFunction, convertTransferFunction(tf));
    EXPECT_FALSE(description->value().isWindows);
}

void CColorManagementTest::expectLuminances(PImageDescription description, float min, uint32_t max, uint32_t reference) {
    ASSERT_TRUE(description);
    const auto& settings = description->value();
    EXPECT_FLOAT_EQ(settings.luminances.min, min);
    EXPECT_EQ(settings.luminances.max, max);
    EXPECT_EQ(settings.luminances.reference, reference);
    EXPECT_FLOAT_EQ(settings.getTFMinLuminance(), min);
    EXPECT_FLOAT_EQ(settings.getTFMaxLuminance(), max);
    EXPECT_FLOAT_EQ(settings.getTFRefLuminance(), reference);
}

TEST_F(CColorManagementTest, OmittedLinearLuminancesUseSDRDefaults) {
    PImageDescription description;
    ASSERT_NO_FATAL_FAILURE(createParametric(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR, description));
    ASSERT_NO_FATAL_FAILURE(expectLuminances(description, 0.2F, 80, 80));
}

class CColorManagementOrderTest : public CColorManagementTest, public ::testing::WithParamInterface<bool> {};

TEST_P(CColorManagementOrderTest, ExplicitLinearLuminances) {
    PImageDescription             description;
    const std::array<uint32_t, 3> luminances = {
        0,
        10000,
        203,
    };
    ASSERT_NO_FATAL_FAILURE(createParametric(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR, description, luminances, GetParam()));
    ASSERT_NO_FATAL_FAILURE(expectLuminances(description, 0.F, 10000, 203));
}

TEST_P(CColorManagementOrderTest, ExplicitLinearNonzeroMinimum) {
    PImageDescription             description;
    const std::array<uint32_t, 3> luminances = {
        2500,
        400,
        120,
    };
    ASSERT_NO_FATAL_FAILURE(createParametric(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR, description, luminances, GetParam()));
    ASSERT_NO_FATAL_FAILURE(expectLuminances(description, 0.25F, 400, 120));
}

TEST_P(CColorManagementOrderTest, ExplicitLinearDefaultsMatchOmittedDescription) {
    PImageDescription             omitted, explicitDefaults;
    const std::array<uint32_t, 3> luminances = {
        2000,
        80,
        80,
    };
    ASSERT_NO_FATAL_FAILURE(createParametric(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR, omitted));
    ASSERT_NO_FATAL_FAILURE(createParametric(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR, explicitDefaults, luminances, GetParam()));
    ASSERT_NO_FATAL_FAILURE(expectLuminances(explicitDefaults, 0.2F, 80, 80));
    EXPECT_EQ(omitted->id(), explicitDefaults->id());
}

INSTANTIATE_TEST_SUITE_P(RequestOrder, CColorManagementOrderTest, ::testing::Bool());

TEST_F(CColorManagementTest, WindowsScRGBKeepsFixedEncoding) {
    ASSERT_NO_FATAL_FAILURE(sendRequest({
        2,
        (12U << 16) | 6U,
        3,
    }));
    PImageDescription description;
    ASSERT_NO_FATAL_FAILURE(readDescription(3, description));
    const auto& settings = description->value();
    EXPECT_TRUE(settings.isWindows);
    EXPECT_EQ(settings.transferFunction, CM_TRANSFER_FUNCTION_EXT_LINEAR);
    EXPECT_TRUE(settings.primariesNameSet);
    EXPECT_EQ(settings.primariesNamed, CM_PRIMARIES_SRGB);
    // Stored metadata retains its default minimum, but Windows encoding starts at zero.
    EXPECT_FLOAT_EQ(settings.luminances.min, 0.2F);
    EXPECT_EQ(settings.luminances.max, 80U);
    EXPECT_EQ(settings.luminances.reference, 203U);
    EXPECT_FLOAT_EQ(settings.getTFMinLuminance(), 0.F);
    EXPECT_FLOAT_EQ(settings.getTFMaxLuminance(), 80.F);
    EXPECT_FLOAT_EQ(settings.getTFRefLuminance(), 203.F);
}

TEST_F(CColorManagementTest, OmittedPQLuminancesKeepHDRDefaults) {
    PImageDescription description;
    ASSERT_NO_FATAL_FAILURE(createParametric(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ, description));
    ASSERT_NO_FATAL_FAILURE(expectLuminances(description, 0.005F, 10000, 203));
}

TEST_F(CColorManagementTest, OmittedHLGLuminancesKeepHDRDefaults) {
    PImageDescription description;
    ASSERT_NO_FATAL_FAILURE(createParametric(WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG, description));
    ASSERT_NO_FATAL_FAILURE(expectLuminances(description, 0.005F, 1000, 203));
}
