#include <Compositor.hpp>
#include <event/EventBus.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <protocols/ColorRepresentation.hpp>
#include <protocols/PresentationTime.hpp>
#include <protocols/core/Compositor.hpp>
#include <state/MonitorState.hpp>

#include <gtest/gtest.h>
#include <array>
#include <initializer_list>
#include <memory>
#include <optional>
#include <sys/socket.h>
#include <vector>

using namespace Hyprutils::OS;
using namespace NColorManagement;

class CColorRepresentationTestBuffer : public IHLBuffer {
  public:
    Aquamarine::eBufferCapability caps() override {
        return Aquamarine::BUFFER_CAPABILITY_NONE;
    }
    Aquamarine::eBufferType type() override {
        return Aquamarine::BUFFER_TYPE_MISC;
    }
    void update(const CRegion&) override {
        ADD_FAILURE();
    }
    bool isSynchronous() override {
        return false;
    }
    bool good() override {
        return true;
    }
};

// Dispatch real requests on an isolated server, without a renderer or GPU.
class CColorRepresentationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        std::array<int, 2> sockets = {
            -1,
            -1,
        };
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        CFileDescriptor server{sockets[0]};
        m_socket = CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), server.get());
        ASSERT_NE(m_client, nullptr);
        server.take();

        m_previousCompositor          = std::move(g_pCompositor);
        m_previousEventLoop           = std::move(g_pEventLoopManager);
        m_previousMonitorState        = std::move(State::monitorState());
        m_previousEventBus            = std::move(Event::bus());
        m_previousPresentation        = std::move(PROTO::presentation);
        m_previousCoreCompositor      = std::move(PROTO::compositor);
        m_previousColorRepresentation = std::move(PROTO::colorRepresentation);
        m_installedGlobals            = true;

        Event::bus()               = makeUnique<Event::CEventBus>();
        State::monitorState()      = makeUnique<State::CMonitorStateTracker>();
        g_pCompositor              = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay = m_display.get();
        g_pEventLoopManager        = makeUnique<CEventLoopManager>(m_display.get(), wl_display_get_event_loop(m_display.get()));
        PROTO::presentation        = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 2, "color-representation-test-presentation");
        PROTO::compositor          = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "color-representation-test-compositor");
        PROTO::colorRepresentation = makeUnique<CColorRepresentationProtocol>(&wp_color_representation_manager_v1_interface, 1, "color-representation-test");

        PROTO::colorRepresentation->bindManager(m_client, nullptr, 1, m_manager);
    }

    void TearDown() override {
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        if (!m_installedGlobals)
            return;

        PROTO::colorRepresentation = std::move(m_previousColorRepresentation);
        PROTO::compositor          = std::move(m_previousCoreCompositor);
        PROTO::presentation        = std::move(m_previousPresentation);
        g_pEventLoopManager        = std::move(m_previousEventLoop);
        g_pCompositor              = std::move(m_previousCompositor);
        State::monitorState()      = std::move(m_previousMonitorState);
        Event::bus()               = std::move(m_previousEventBus);
    }

    SP<CWLSurfaceResource> surface() {
        auto surface          = makeShared<CWLSurfaceResource>(makeShared<CWlSurface>(m_client, 6, m_nextID++));
        surface->m_self       = surface;
        surface->m_stateQueue = CSurfaceStateQueue{surface};
        return surface;
    }

    SP<CColorRepresentationTestBuffer> buffer() {
        auto buffer                  = makeShared<CColorRepresentationTestBuffer>();
        buffer->m_resource           = CWLBufferResource::create(makeShared<CWlBuffer>(m_client, 1, m_nextID++));
        buffer->m_resource->m_buffer = buffer;
        buffer->size                 = {16, 16};
        return buffer;
    }

    uint32_t representation(SP<CWLSurfaceResource> surface) {
        const auto id = m_nextID++;
        request(m_manager, 1, {id, surface->id()}); // wp_color_representation_manager_v1.get_surface
        return id;
    }

    void request(uint32_t id, uint32_t opcode, std::initializer_list<uint32_t> args = {}) {
        std::vector<uint32_t> message = {
            id,
            sc<uint32_t>((args.size() + 2) * sizeof(uint32_t) << 16) | opcode,
        };
        message.insert(message.end(), args);
        const auto size = message.size() * sizeof(uint32_t);
        ASSERT_EQ(send(m_socket.get(), message.data(), size, MSG_NOSIGNAL), sc<ssize_t>(size));
        ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    }

    void commit(SP<CWLSurfaceResource> surface) {
        request(surface->id(), 6); // wl_surface.commit
    }

    // returns {object, code} of wl_display.error, skipping the events sent before it
    std::optional<std::pair<uint32_t, uint32_t>> protocolError() {
        wl_display_flush_clients(m_display.get());

        std::vector<uint32_t> words(4096);
        const auto            received = recv(m_socket.get(), words.data(), words.size() * sizeof(uint32_t), MSG_DONTWAIT);
        if (received <= 0)
            return std::nullopt;

        words.resize(received / sizeof(uint32_t));
        for (size_t i = 0; i + 3 < words.size();) {
            const auto SIZE = words[i + 1] >> 16;
            if (SIZE < 8)
                break;

            if (words[i] == 1 && (words[i + 1] & 0xFFFF) == 0) // wl_display.error
                return std::pair{words[i + 2], words[i + 3]};

            i += SIZE / sizeof(uint32_t);
        }

        return std::nullopt;
    }

    wl_client* m_client  = nullptr; // Owned by m_display.
    uint32_t   m_manager = 2;
    uint32_t   m_nextID  = 3;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
    CFileDescriptor                                            m_socket;
    bool                                                       m_installedGlobals = false;
    UP<CCompositor>                                            m_previousCompositor;
    UP<CEventLoopManager>                                      m_previousEventLoop;
    UP<State::CMonitorStateTracker>                            m_previousMonitorState;
    UP<Event::CEventBus>                                       m_previousEventBus;
    UP<CPresentationProtocol>                                  m_previousPresentation;
    UP<CWLCompositorProtocol>                                  m_previousCoreCompositor;
    UP<CColorRepresentationProtocol>                           m_previousColorRepresentation;
};

TEST_F(CColorRepresentationTest, StateAppliesOnCommit) {
    auto       surf = surface();
    const auto id   = representation(surf);

    request(id, 2, {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED});
    request(id, 3, {WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_2});
    EXPECT_EQ(surf->m_current.colorRepresentation, SColorRepresentation{});

    commit(surf);
    EXPECT_EQ(surf->m_current.colorRepresentation, (SColorRepresentation{CM_YCBCR_COEFFICIENTS_BT2020, CM_YCBCR_RANGE_LIMITED, CM_CHROMA_LOCATION_TYPE_2}));
    EXPECT_FALSE(protocolError());
}

TEST_F(CColorRepresentationTest, DestroyUnsetsOnCommit) {
    auto       surf = surface();
    const auto id   = representation(surf);

    request(id, 2, {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL});
    commit(surf);
    ASSERT_NE(surf->m_current.colorRepresentation, SColorRepresentation{});

    request(id, 0); // destroy
    EXPECT_NE(surf->m_current.colorRepresentation, SColorRepresentation{});

    commit(surf);
    EXPECT_EQ(surf->m_current.colorRepresentation, SColorRepresentation{});

    // the surface can get a new one now
    representation(surf);
    EXPECT_FALSE(protocolError());
}

TEST_F(CColorRepresentationTest, SecondObjectForSurfaceIsAnError) {
    auto surf = surface();
    representation(surf);
    representation(surf);
    EXPECT_EQ(protocolError(), std::pair(m_manager, sc<uint32_t>(WP_COLOR_REPRESENTATION_MANAGER_V1_ERROR_SURFACE_EXISTS)));
}

TEST_F(CColorRepresentationTest, OnlyPremultipliedElectricalAlpha) {
    auto       surf = surface();
    const auto id   = representation(surf);
    request(id, 1, {WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL});
    EXPECT_FALSE(protocolError());

    request(id, 1, {WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_STRAIGHT});
    EXPECT_EQ(protocolError(), std::pair(id, sc<uint32_t>(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_ALPHA_MODE)));
}

TEST_F(CColorRepresentationTest, UnadvertisedCoefficientsAreAnError) {
    auto       surf = surface();
    const auto id   = representation(surf);
    request(id, 2, {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_ICTCP, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED});
    EXPECT_EQ(protocolError(), std::pair(id, sc<uint32_t>(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_COEFFICIENTS)));
}

TEST_F(CColorRepresentationTest, InvalidChromaLocationIsAnError) {
    auto       surf = surface();
    const auto id   = representation(surf);
    request(id, 3, {WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_5 + 1});
    EXPECT_EQ(protocolError(), std::pair(id, sc<uint32_t>(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_CHROMA_LOCATION)));
}

TEST_F(CColorRepresentationTest, NonYUVBufferIsAnError) {
    auto       surf = surface();
    auto       buf  = buffer();
    const auto id   = representation(surf);

    request(id, 2, {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED});
    request(surf->id(), 1, {wl_resource_get_id(buf->m_resource->getResource()), 0, 0}); // wl_surface.attach
    commit(surf);
    EXPECT_EQ(protocolError(), std::pair(id, sc<uint32_t>(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_PIXEL_FORMAT)));
    EXPECT_EQ(surf->m_current.colorRepresentation, SColorRepresentation{});
}

TEST_F(CColorRepresentationTest, RequestsAfterSurfaceDestructionAreAnError) {
    auto       surf = surface();
    const auto id   = representation(surf);

    surf.reset();
    request(id, 1, {WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL});
    EXPECT_EQ(protocolError(), std::pair(id, sc<uint32_t>(WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT)));
}
