#include <protocols/core/DataDevice.hpp>
#include <protocols/core/Seat.hpp>
#include <protocols/CursorShape.hpp>
#include <protocols/IdleInhibit.hpp>
#include <protocols/VirtualKeyboard.hpp>
#include <protocols/VirtualPointer.hpp>
#include <protocols/TextInputV1.hpp>
#include <protocols/TextInputV3.hpp>
#include <protocols/InputMethodV2.hpp>
#include <Compositor.hpp>
#include <event/EventBus.hpp>
#include <managers/SeatManager.hpp>
#include <managers/eventLoop/EventLoopManager.hpp>
#include <managers/input/InputManager.hpp>

#include <gtest/gtest.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <sys/socket.h>

using namespace Hyprutils::OS;

static constexpr char     MIME[] = "text/plain";

static constexpr uint32_t COPY = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
static constexpr uint32_t MOVE = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
static constexpr uint32_t ASK  = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;

class CTestDataSource : public IDataSource {
  public:
    std::vector<std::string> mimes() override {
        return {MIME};
    }
    void send(const std::string& mime, CFileDescriptor fd) override {
        EXPECT_TRUE(m_allowSend) << "Unexpected data transfer";
        m_sentMime = mime;
        m_sentFd   = std::move(fd);
    }
    void accepted(const std::string&) override {
        ;
    }
    void cancelled() override {
        ++m_cancelled;
    }
    void error(uint32_t, const std::string&) override {
        ADD_FAILURE() << "Unexpected source error";
    }
    bool hasDnd() override {
        return m_dnd;
    }
    uint32_t actions() override {
        return m_supported;
    }
    void sendDndAction(wl_data_device_manager_dnd_action action) override {
        m_actions.push_back(action);
    }
    void sendDndDropPerformed() override {
        ++m_drops;
    }
    void sendDndFinished() override {
        m_actionAtFinish = m_actions.empty() ? 0 : m_actions.back();
        ++m_finished;
    }

    bool                  m_allowSend = false;
    std::string           m_sentMime;
    CFileDescriptor       m_sentFd;
    uint32_t              m_supported      = COPY | MOVE | ASK;
    uint32_t              m_actionAtFinish = 0;
    bool                  m_dnd            = true;
    int                   m_drops          = 0;
    int                   m_finished       = 0;
    int                   m_cancelled      = 0;
    std::vector<uint32_t> m_actions;
};

// wl_data_offer requests and events on an isolated server
class CDataDeviceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        m_display.reset(wl_display_create());
        ASSERT_TRUE(m_display);
        std::array<int, 2> sockets = {-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
        CFileDescriptor server{sockets[0]};
        m_socket = CFileDescriptor{sockets[1]};
        m_client = wl_client_create(m_display.get(), server.get());
        ASSERT_NE(m_client, nullptr);
        server.take();
        m_source = makeShared<CTestDataSource>();
    }

    void TearDown() override {
        if (m_display)
            wl_display_destroy_clients(m_display.get());
        m_offer.reset();
        m_wire.reset();
    }

    void offer(uint32_t version = 3, SP<IDataSource> source = nullptr) {
        m_wire  = makeShared<CWlDataOffer>(m_client, version, m_offerId);
        m_offer = makeShared<CWLDataOfferResource>(m_wire, source ? source : m_source);
        ASSERT_TRUE(m_offer->good());
        m_offer->m_self = m_offer;
        // the fixture owns these offers
        m_wire->setOnDestroy([](CWlDataOffer*) { ; });
        m_offer->sendData();
    }

    void request(uint32_t opcode, const std::vector<uint32_t>& args = {}, uint32_t object = 0, int fd = -1) {
        std::vector<uint32_t> message = {
            object ? object : m_offerId,
            sc<uint32_t>((args.size() + 2) * sizeof(uint32_t) << 16) | opcode,
        };
        message.append_range(args);
        const auto                                                      size    = message.size() * sizeof(uint32_t);
        iovec                                                           iov     = {.iov_base = message.data(), .iov_len = size};
        msghdr                                                          msg     = {.msg_iov = &iov, .msg_iovlen = 1};
        alignas(cmsghdr) std::array<std::byte, CMSG_SPACE(sizeof(int))> control = {};
        if (fd >= 0) {
            msg.msg_control    = control.data();
            msg.msg_controllen = control.size();
            const auto cmsg    = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_level   = SOL_SOCKET;
            cmsg->cmsg_type    = SCM_RIGHTS;
            cmsg->cmsg_len     = CMSG_LEN(sizeof(int));
            std::memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
        }
        ASSERT_EQ(sendmsg(m_socket.get(), &msg, MSG_NOSIGNAL), sc<ssize_t>(size));
        ASSERT_EQ(wl_event_loop_dispatch(wl_display_get_event_loop(m_display.get()), 0), 0);
    }

    void setActions(uint32_t actions, uint32_t preferred) {
        request(4, {actions, preferred});
    }

    void expectActions(std::initializer_list<uint32_t> actions) {
        expectActions(actions, actions);
    }

    void expectActions(std::initializer_list<uint32_t> destinationActions, std::initializer_list<uint32_t> sourceActions) {
        wl_client_flush(m_client);
        std::array<uint32_t, 256> buffer = {};
        const auto                size   = recv(m_socket.get(), buffer.data(), sizeof(buffer), MSG_DONTWAIT);
        ASSERT_GT(size, 0);
        std::vector<uint32_t> received;
        ASSERT_EQ(size % sizeof(uint32_t), 0U);
        for (size_t i = 0; i < sc<size_t>(size) / sizeof(uint32_t);) {
            ASSERT_LT(i + 1, sc<size_t>(size) / sizeof(uint32_t));
            const auto bytes  = buffer[i + 1] >> 16;
            const auto opcode = buffer[i + 1] & 0xffff;
            ASSERT_GE(bytes, 8U);
            ASSERT_EQ(bytes % sizeof(uint32_t), 0U);
            ASSERT_LE(i + bytes / sizeof(uint32_t), sc<size_t>(size) / sizeof(uint32_t));
            ASSERT_EQ(buffer[i], m_offerId);
            if (opcode == WL_DATA_OFFER_ACTION) {
                ASSERT_EQ(bytes, 12U);
                received.push_back(buffer[i + 2]);
            } else
                EXPECT_TRUE(opcode == WL_DATA_OFFER_OFFER || opcode == WL_DATA_OFFER_SOURCE_ACTIONS);
            i += bytes / sizeof(uint32_t);
        }
        EXPECT_EQ(received, std::vector<uint32_t>(destinationActions));
        EXPECT_EQ(m_source->m_actions, std::vector<uint32_t>(sourceActions));
    }

    void expectError(uint32_t code, uint32_t object = 0) {
        // the client may be gone after a protocol error
        std::array<uint32_t, 256> buffer = {};
        const auto                size   = recv(m_socket.get(), buffer.data(), sizeof(buffer), MSG_DONTWAIT);
        ASSERT_GT(size, 0);
        ASSERT_EQ(size % sizeof(uint32_t), 0U);
        bool found = false;
        for (size_t i = 0; i < sc<size_t>(size) / sizeof(uint32_t);) {
            ASSERT_LT(i + 1, sc<size_t>(size) / sizeof(uint32_t));
            const auto bytes = buffer[i + 1] >> 16;
            ASSERT_GE(bytes, 8U);
            ASSERT_EQ(bytes % sizeof(uint32_t), 0U);
            ASSERT_LE(i + bytes / sizeof(uint32_t), sc<size_t>(size) / sizeof(uint32_t));
            if (buffer[i] == 1 && (buffer[i + 1] & 0xffff) == WL_DISPLAY_ERROR) {
                ASSERT_GE(bytes, 20U);
                EXPECT_EQ(buffer[i + 2], object ? object : m_offerId);
                EXPECT_EQ(buffer[i + 3], code);
                found = true;
            }
            i += bytes / sizeof(uint32_t);
        }
        EXPECT_TRUE(found);
    }

    void accept(bool accepted = true) {
        if (!accepted) {
            request(0, {0, 0}); // serial, NULL MIME
            return;
        }

        std::vector<uint32_t> args(2 + (sizeof(MIME) + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
        args[1] = sizeof(MIME); // includes the null terminator
        std::memcpy(args.data() + 2, MIME, sizeof(MIME));
        request(0, args); // wl_data_offer.accept
    }

    // sync must still work after the queued requests
    void expectClientAlive() {
        ASSERT_NO_FATAL_FAILURE(request(0, {m_offerId + 1}, 1)); // wl_display.sync
        wl_display_flush_clients(display());
        std::array<uint32_t, 256> buffer = {};
        const auto                size   = recv(m_socket.get(), buffer.data(), sizeof(buffer), MSG_DONTWAIT);
        ASSERT_GT(size, 0);
        bool done = false;
        for (size_t i = 0; i < sc<size_t>(size) / sizeof(uint32_t);) {
            ASSERT_LT(i + 1, sc<size_t>(size) / sizeof(uint32_t));
            const auto bytes = buffer[i + 1] >> 16;
            ASSERT_GE(bytes, 8U);
            ASSERT_EQ(bytes % sizeof(uint32_t), 0U);
            ASSERT_LE(i + bytes / sizeof(uint32_t), sc<size_t>(size) / sizeof(uint32_t));
            EXPECT_FALSE(buffer[i] == 1 && (buffer[i + 1] & 0xffff) == WL_DISPLAY_ERROR);
            done |= buffer[i] == m_offerId + 1 && (buffer[i + 1] & 0xffff) == WL_CALLBACK_DONE;
            i += bytes / sizeof(uint32_t);
        }
        EXPECT_TRUE(done);
    }

    wl_display* display() {
        return m_display.get();
    }

    void drop() {
        m_offer->m_dropped = true;
        m_offer->m_inAsk   = m_offer->m_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }

    uint32_t selectedAction() {
        return m_offer->m_action;
    }

    SP<CTestDataSource>      m_source;
    SP<CWLDataOfferResource> m_offer;
    SP<CWlDataOffer>         m_wire;
    wl_client*               m_client = nullptr; // owned by m_display
    CFileDescriptor          m_socket;
    uint32_t                 m_offerId = 2;

  private:
    std::unique_ptr<wl_display, decltype(&wl_display_destroy)> m_display{nullptr, wl_display_destroy};
};

TEST_F(CDataDeviceTest, CopyOnlyDestinationNotifiesBothSides) {
    offer();
    setActions(COPY, COPY);
    expectActions({COPY});
}

TEST_F(CDataDeviceTest, MoveOnlyDestinationNotifiesBothSides) {
    offer();
    setActions(MOVE, MOVE);
    expectActions({MOVE});
}

TEST_F(CDataDeviceTest, PreferenceChangesAndRejectionNotifyBothSidesWithoutDuplicates) {
    offer();
    setActions(COPY | MOVE, COPY);
    setActions(COPY | MOVE, COPY);
    setActions(COPY | MOVE, MOVE);
    setActions(0, 0);
    expectActions({COPY, MOVE, 0});
}

TEST_F(CDataDeviceTest, UnsupportedPreferenceFallsBackToCommonAction) {
    m_source->m_supported = MOVE;
    offer();
    setActions(COPY | MOVE, COPY);
    expectActions({MOVE});
}

TEST_F(CDataDeviceTest, NoCommonActionDoesNotInventMove) {
    m_source->m_supported = MOVE;
    offer();
    setActions(COPY, COPY);
    EXPECT_EQ(selectedAction(), 0U);
    expectActions({});
}

TEST_F(CDataDeviceTest, ZeroSourceActionsStayUnsupported) {
    m_source->m_supported = 0;
    offer();
    setActions(COPY, COPY);
    expectActions({});
}

TEST_F(CDataDeviceTest, NoDestinationActionsMeansNoSelection) {
    offer();
    expectActions({});
}

TEST_F(CDataDeviceTest, MissingPreferenceUsesCopyFallback) {
    offer();
    setActions(COPY | MOVE | ASK, 0);
    expectActions({COPY});
}

TEST_F(CDataDeviceTest, LegacyDestinationUsesCopyWithoutVersionThreeEvents) {
    offer(2);
    EXPECT_EQ(selectedAction(), COPY);
    expectActions({}, {COPY});
}

TEST_F(CDataDeviceTest, DeadOfferCannotChangeSourceAction) {
    offer();
    setActions(COPY, COPY);
    m_offer->m_dead = true;
    setActions(MOVE, MOVE);
    expectActions({COPY});
}

TEST_F(CDataDeviceTest, NormalDropFreezesAction) {
    offer();
    setActions(COPY, COPY);
    drop();
    setActions(MOVE, MOVE);
    expectActions({COPY});
}

TEST_F(CDataDeviceTest, AskDefersFinalSourceNotificationAndSendsNoPostDropOfferAction) {
    offer();
    setActions(COPY | MOVE | ASK, ASK);
    drop();
    setActions(MOVE, MOVE);
    EXPECT_EQ(selectedAction(), MOVE);
    expectActions({ASK});
}

TEST_F(CDataDeviceTest, InvalidActionMaskRaisesProtocolError) {
    offer();
    setActions(8, 0);
    expectError(WL_DATA_OFFER_ERROR_INVALID_ACTION_MASK);
}

TEST_F(CDataDeviceTest, MultiplePreferredActionsRaiseProtocolError) {
    offer();
    setActions(COPY | MOVE, COPY | MOVE);
    expectError(WL_DATA_OFFER_ERROR_INVALID_ACTION);
}

TEST_F(CDataDeviceTest, PreferredActionOutsideDestinationMaskRaisesProtocolError) {
    offer();
    setActions(COPY, MOVE);
    expectError(WL_DATA_OFFER_ERROR_INVALID_ACTION);
}

TEST_F(CDataDeviceTest, ClipboardOfferRejectsActions) {
    m_source->m_dnd = false;
    offer();
    setActions(COPY, COPY);
    expectError(WL_DATA_OFFER_ERROR_INVALID_OFFER);
}

TEST_F(CDataDeviceTest, PrematureFinishRaisesProtocolError) {
    offer();
    setActions(COPY, COPY);
    request(3);
    expectError(WL_DATA_OFFER_ERROR_INVALID_FINISH);
    EXPECT_EQ(m_source->m_finished, 0);
}

TEST_F(CDataDeviceTest, ClipboardOfferRejectsFinish) {
    m_source->m_dnd = false;
    offer();
    request(3);
    expectError(WL_DATA_OFFER_ERROR_INVALID_FINISH);
}

TEST_F(CDataDeviceTest, AskRejectsFinalActionUnsupportedBySource) {
    m_source->m_supported = ASK | COPY;
    offer();
    setActions(ASK, ASK);
    drop();
    setActions(MOVE, MOVE);
    expectError(WL_DATA_OFFER_ERROR_INVALID_ACTION);
}

// drop completion calls back into the input manager
class CDataDeviceDragTest : public CDataDeviceTest {
  protected:
    void SetUp() override {
        ASSERT_NO_FATAL_FAILURE(CDataDeviceTest::SetUp());
        m_previousCompositor            = std::move(g_pCompositor);
        m_previousEventLoop             = std::move(g_pEventLoopManager);
        m_previousEventBus              = std::move(Event::bus());
        m_previousSeat                  = std::move(PROTO::seat);
        m_previousSeatManager           = std::move(g_pSeatManager);
        m_previousCursorShape           = std::move(PROTO::cursorShape);
        m_previousIdleInhibit           = std::move(PROTO::idleInhibit);
        m_previousVirtualKeyboard       = std::move(PROTO::virtualKeyboard);
        m_previousVirtualPointer        = std::move(PROTO::virtualPointer);
        m_previousTextInputV1           = std::move(PROTO::textInputV1);
        m_previousTextInputV3           = std::move(PROTO::textInputV3);
        m_previousIME                   = std::move(PROTO::ime);
        m_previousInput                 = std::move(g_pInputManager);
        m_previousData                  = std::move(PROTO::data);
        m_installedGlobals              = true;
        Event::bus()                    = makeUnique<Event::CEventBus>();
        g_pCompositor                   = makeUnique<CCompositor>(true);
        g_pCompositor->m_wlDisplay      = display();
        g_pCompositor->m_isShuttingDown = true; // skip input hit testing
        g_pEventLoopManager             = makeUnique<CEventLoopManager>(display(), wl_display_get_event_loop(display()));
        PROTO::seat                     = makeUnique<CWLSeatProtocol>(&wl_seat_interface, 9, "dnd-test-seat");
        g_pSeatManager                  = makeUnique<CSeatManager>();
        PROTO::cursorShape              = makeUnique<CCursorShapeProtocol>(&wp_cursor_shape_manager_v1_interface, 1, "dnd-test-cursor");
        PROTO::idleInhibit              = makeUnique<CIdleInhibitProtocol>(&zwp_idle_inhibit_manager_v1_interface, 1, "dnd-test-idle");
        PROTO::virtualKeyboard          = makeUnique<CVirtualKeyboardProtocol>(&zwp_virtual_keyboard_manager_v1_interface, 1, "dnd-test-keyboard");
        PROTO::virtualPointer           = makeUnique<CVirtualPointerProtocol>(&zwlr_virtual_pointer_manager_v1_interface, 1, "dnd-test-pointer");
        PROTO::textInputV1              = makeUnique<CTextInputV1Protocol>(&zwp_text_input_manager_v1_interface, 1, "dnd-test-text-v1");
        PROTO::textInputV3              = makeUnique<CTextInputV3Protocol>(&zwp_text_input_manager_v3_interface, 1, "dnd-test-text-v3");
        PROTO::ime                      = makeUnique<CInputMethodV2Protocol>(&zwp_input_method_manager_v2_interface, 1, "dnd-test-ime");
        g_pInputManager                 = makeUnique<CInputManager>();
        PROTO::data                     = makeUnique<CWLDataDeviceProtocol>(&wl_data_device_manager_interface, 3, "dnd-test-data");
        m_deviceWire                    = makeShared<CWlDataDevice>(m_client, 3, 0);
        m_device                        = makeShared<CWLDataDeviceResource>(m_deviceWire);
        ASSERT_TRUE(m_device->good());
        m_deviceWire->setOnDestroy([](CWlDataDevice*) { ; });
    }

    void TearDown() override {
        CDataDeviceTest::TearDown();
        m_realSource.reset();
        m_sourceWire.reset();
        m_device.reset();
        m_deviceWire.reset();
        if (!m_installedGlobals)
            return;

        PROTO::data            = std::move(m_previousData);
        g_pInputManager        = std::move(m_previousInput);
        PROTO::ime             = std::move(m_previousIME);
        PROTO::textInputV3     = std::move(m_previousTextInputV3);
        PROTO::textInputV1     = std::move(m_previousTextInputV1);
        PROTO::virtualPointer  = std::move(m_previousVirtualPointer);
        PROTO::virtualKeyboard = std::move(m_previousVirtualKeyboard);
        PROTO::idleInhibit     = std::move(m_previousIdleInhibit);
        PROTO::cursorShape     = std::move(m_previousCursorShape);
        g_pSeatManager         = std::move(m_previousSeatManager);
        PROTO::seat            = std::move(m_previousSeat);
        g_pEventLoopManager    = std::move(m_previousEventLoop);
        g_pCompositor          = std::move(m_previousCompositor);
        Event::bus()           = std::move(m_previousEventBus);
    }

    void currentOffer(uint32_t version = 3) {
        ASSERT_NO_FATAL_FAILURE(offer(version));
        PROTO::data->m_offers.emplace_back(m_offer);
        PROTO::data->m_dnd.currentSource = m_source;
        PROTO::data->m_dnd.currentOffer  = m_offer;
        PROTO::data->m_dnd.focusedDevice = m_device;
        m_wire->setDestroy([this](CWlDataOffer*) { PROTO::data->destroyResource(m_offer.get()); });
        m_wire->setOnDestroy([this](CWlDataOffer*) { PROTO::data->destroyResource(m_offer.get()); });
    }

    void realSource(uint32_t version = 3) {
        m_offerId    = 3;
        m_sourceWire = makeShared<CWlDataSource>(m_client, version, 2);
        m_realSource = makeShared<CWLDataSourceResource>(m_sourceWire, nullptr);
        ASSERT_TRUE(m_realSource->good());
        m_realSource->m_self = m_realSource;
        PROTO::data->m_sources.emplace_back(m_realSource);
        std::vector<uint32_t> mime(1 + (sizeof(MIME) + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
        mime[0] = sizeof(MIME);
        std::memcpy(mime.data() + 1, MIME, sizeof(MIME));
        request(0, mime, 2); // wl_data_source.offer
    }

    void realSourceOffer(uint32_t destinationVersion = 3) {
        m_realSource->m_dnd = true;
        m_realSource->markUsed();
        ASSERT_NO_FATAL_FAILURE(offer(destinationVersion, m_realSource));
        PROTO::data->m_offers.emplace_back(m_offer);
        PROTO::data->m_dnd.currentSource = m_realSource;
        PROTO::data->m_dnd.currentOffer  = m_offer;
        PROTO::data->m_dnd.focusedDevice = m_device;
        m_wire->setDestroy([this](CWlDataOffer*) { PROTO::data->destroyResource(m_offer.get()); });
        m_wire->setOnDestroy([this](CWlDataOffer*) { PROTO::data->destroyResource(m_offer.get()); });
    }

    void expectSourceEvents(std::initializer_list<std::pair<uint32_t, uint32_t>> expected) {
        wl_client_flush(m_client);
        std::array<uint32_t, 256>                  buffer = {};
        const auto                                 size   = recv(m_socket.get(), buffer.data(), sizeof(buffer), MSG_DONTWAIT);
        std::vector<std::pair<uint32_t, uint32_t>> events;
        if (size < 0) {
            EXPECT_EQ(errno, EAGAIN);
            EXPECT_EQ(expected.size(), 0U);
            return;
        }
        ASSERT_GT(size, 0);
        for (size_t i = 0; i < sc<size_t>(size) / sizeof(uint32_t);) {
            ASSERT_LT(i + 1, sc<size_t>(size) / sizeof(uint32_t));
            const auto bytes  = buffer[i + 1] >> 16;
            const auto opcode = buffer[i + 1] & 0xffff;
            ASSERT_GE(bytes, 8U);
            ASSERT_EQ(bytes % sizeof(uint32_t), 0U);
            ASSERT_LE(i + bytes / sizeof(uint32_t), sc<size_t>(size) / sizeof(uint32_t));
            EXPECT_FALSE(buffer[i] == 1 && opcode == WL_DISPLAY_ERROR);
            if (buffer[i] == 2) {
                if (opcode == WL_DATA_SOURCE_ACTION) {
                    ASSERT_EQ(bytes, 12U);
                    events.emplace_back(opcode, buffer[i + 2]);
                } else if (opcode == WL_DATA_SOURCE_CANCELLED || opcode == WL_DATA_SOURCE_DND_DROP_PERFORMED || opcode == WL_DATA_SOURCE_DND_FINISHED) {
                    ASSERT_EQ(bytes, 8U);
                    events.emplace_back(opcode, 0);
                }
            }
            i += bytes / sizeof(uint32_t);
        }
        EXPECT_EQ(events, (std::vector<std::pair<uint32_t, uint32_t>>(expected)));
    }

    void beginAnotherDrag() {
        m_nextSource                     = makeShared<CTestDataSource>();
        PROTO::data->m_dnd.currentSource = m_nextSource;
    }

    void abortDrag() {
        PROTO::data->abortDrag();
    }

    void destroyOffer() {
        ASSERT_NO_FATAL_FAILURE(request(2)); // wl_data_offer.destroy
        // drop the fixture's references too
        m_offer.reset();
        m_wire.reset();
        EXPECT_EQ(wl_client_get_object(m_client, m_offerId), nullptr);
    }

    void performDrop() {
        PROTO::data->dropDrag();
    }

    void leaveTarget() {
        PROTO::data->updateDrag();
    }

    SP<CWLDataSourceResource> m_realSource;
    SP<CWlDataSource>         m_sourceWire;
    SP<CTestDataSource>       m_nextSource;

  private:
    bool                         m_installedGlobals = false;
    SP<CWlDataDevice>            m_deviceWire;
    SP<CWLDataDeviceResource>    m_device;
    UP<CCompositor>              m_previousCompositor;
    UP<CEventLoopManager>        m_previousEventLoop;
    UP<Event::CEventBus>         m_previousEventBus;
    UP<CWLSeatProtocol>          m_previousSeat;
    UP<CSeatManager>             m_previousSeatManager;
    UP<CCursorShapeProtocol>     m_previousCursorShape;
    UP<CIdleInhibitProtocol>     m_previousIdleInhibit;
    UP<CVirtualKeyboardProtocol> m_previousVirtualKeyboard;
    UP<CVirtualPointerProtocol>  m_previousVirtualPointer;
    UP<CTextInputV1Protocol>     m_previousTextInputV1;
    UP<CTextInputV3Protocol>     m_previousTextInputV3;
    UP<CInputMethodV2Protocol>   m_previousIME;
    UP<CInputManager>            m_previousInput;
    UP<CWLDataDeviceProtocol>    m_previousData;
};

TEST_F(CDataDeviceDragTest, MoveFinishesOnlyAfterDestinationFinish) {
    currentOffer();
    setActions(MOVE, MOVE);
    accept();
    performDrop();
    EXPECT_EQ(m_source->m_drops, 1);
    EXPECT_EQ(m_source->m_finished, 0);
    EXPECT_FALSE(PROTO::data->dndActive());
    request(3); // same-client transfers can finish without receive
    EXPECT_EQ(m_source->m_drops, 1);
    EXPECT_EQ(m_source->m_finished, 1);
    EXPECT_EQ(m_source->m_cancelled, 0);
    EXPECT_FALSE(PROTO::data->dndActive());
}

TEST_F(CDataDeviceDragTest, NoCommonActionCancelsInsteadOfDropping) {
    m_source->m_supported = MOVE;
    currentOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    EXPECT_EQ(m_source->m_drops, 0);
    EXPECT_EQ(m_source->m_finished, 0);
    EXPECT_EQ(m_source->m_cancelled, 1);
}

TEST_F(CDataDeviceDragTest, RejectedMimeCancelsInsteadOfDropping) {
    currentOffer();
    setActions(MOVE, MOVE);
    accept();
    accept(false);
    performDrop();
    EXPECT_EQ(m_source->m_drops, 0);
    EXPECT_EQ(m_source->m_finished, 0);
    EXPECT_EQ(m_source->m_cancelled, 1);
}

TEST_F(CDataDeviceDragTest, DestroyingDroppedModernOfferCancelsInsteadOfFinishing) {
    currentOffer();
    setActions(MOVE, MOVE);
    accept();
    performDrop();
    destroyOffer();
    EXPECT_EQ(m_source->m_finished, 0);
    EXPECT_EQ(m_source->m_cancelled, 1);
}

TEST_F(CDataDeviceDragTest, DestroyingDroppedLegacyOfferCompletes) {
    currentOffer(2);
    performDrop();
    destroyOffer();
    EXPECT_EQ(m_source->m_drops, 1);
    EXPECT_EQ(m_source->m_finished, 1);
    EXPECT_EQ(m_source->m_cancelled, 0);
}

TEST_F(CDataDeviceDragTest, AskReportsFinalActionBeforeFinish) {
    currentOffer();
    setActions(COPY | MOVE | ASK, ASK);
    accept();
    performDrop();
    setActions(MOVE, MOVE);
    EXPECT_EQ(m_source->m_actions, std::vector<uint32_t>{ASK});
    EXPECT_EQ(m_source->m_finished, 0);
    request(3);
    EXPECT_EQ(m_source->m_actions, (std::vector<uint32_t>{ASK, MOVE}));
    EXPECT_EQ(m_source->m_finished, 1);
    EXPECT_EQ(m_source->m_actionAtFinish, MOVE);
}

TEST_F(CDataDeviceDragTest, LeavingTargetClearsActionAndRejectsStaleUpdates) {
    currentOffer();
    setActions(MOVE, MOVE);
    accept();
    leaveTarget();
    setActions(COPY, COPY);
    EXPECT_EQ(m_source->m_actions, (std::vector<uint32_t>{MOVE, 0}));
    EXPECT_EQ(m_source->m_finished, 0);
    EXPECT_EQ(m_source->m_cancelled, 0);
    performDrop();
    EXPECT_EQ(m_source->m_drops, 0);
    EXPECT_EQ(m_source->m_cancelled, 1);
}

TEST_F(CDataDeviceDragTest, DestroyAfterFinishDoesNotSendDuplicateCompletion) {
    currentOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    request(3);
    destroyOffer();
    EXPECT_EQ(m_source->m_finished, 1);
    EXPECT_EQ(m_source->m_cancelled, 0);
}

TEST_F(CDataDeviceTest, OrphanedOfferIgnoresQueuedRequests) {
    offer();
    m_source.reset();
    setActions(COPY, COPY);
    request(3);
    expectClientAlive();
}

TEST_F(CDataDeviceTest, SourceDestroySignalWithdrawsOffer) {
    offer();
    m_source->m_events.destroy.emit();
    EXPECT_TRUE(m_offer->m_dead);
    setActions(COPY, COPY);
    request(3);
    expectClientAlive();
    EXPECT_TRUE(m_source->m_actions.empty());
}

TEST_F(CDataDeviceDragTest, CancelledOfferIgnoresQueuedRequests) {
    currentOffer();
    setActions(COPY, COPY);
    accept();
    abortDrag();
    setActions(8, COPY | MOVE);
    request(3);
    expectClientAlive();
    EXPECT_EQ(m_source->m_cancelled, 1);
    EXPECT_EQ(m_source->m_finished, 0);
}

TEST_F(CDataDeviceDragTest, AskDropReleasesInputBeforeChoosingFinalAction) {
    currentOffer();
    setActions(ASK | COPY, ASK);
    accept();
    ASSERT_TRUE(PROTO::data->dndActive());
    performDrop();
    EXPECT_FALSE(PROTO::data->dndActive());
    PROTO::data->abortDndIfPresent(); // escape must not cancel a dropped transfer
    leaveTarget();
    performDrop(); // a second button release must not send another drop
    EXPECT_EQ(m_source->m_drops, 1);
    EXPECT_EQ(m_source->m_cancelled, 0);
    setActions(COPY, COPY);
    request(3);
    EXPECT_EQ(m_source->m_finished, 1);
}

TEST_F(CDataDeviceDragTest, FinishingDroppedOfferDoesNotFinishNewDrag) {
    currentOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    beginAnotherDrag();
    request(3);
    EXPECT_EQ(m_source->m_finished, 1);
    EXPECT_TRUE(PROTO::data->dndActive());
    EXPECT_EQ(m_nextSource->m_finished, 0);
    EXPECT_EQ(m_nextSource->m_cancelled, 0);
}

TEST_F(CDataDeviceDragTest, DestroyingDroppedOfferDoesNotAbortNewDrag) {
    currentOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    beginAnotherDrag();
    destroyOffer();
    EXPECT_EQ(m_source->m_cancelled, 1);
    EXPECT_TRUE(PROTO::data->dndActive());
    EXPECT_EQ(m_nextSource->m_cancelled, 0);
}

TEST_F(CDataDeviceDragTest, SourceRejectsInvalidActions) {
    realSource();
    request(2, {8}, 2);
    expectError(WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK, 2);
}

TEST_F(CDataDeviceDragTest, SourceRejectsRepeatedActions) {
    realSource();
    request(2, {COPY}, 2);
    request(2, {MOVE}, 2);
    expectError(WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK, 2);
}

TEST_F(CDataDeviceDragTest, SourceRejectsActionsAfterUse) {
    realSource();
    m_realSource->markUsed();
    request(2, {COPY}, 2);
    expectError(WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK, 2);
}

TEST_F(CDataDeviceDragTest, ModernSourceWithoutActionsDoesNotGetLegacyCopyFallback) {
    realSource();
    realSourceOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    expectSourceEvents({{WL_DATA_SOURCE_CANCELLED, 0}});
}

TEST_F(CDataDeviceDragTest, ExplicitZeroSourceActionsDoesNotGetLegacyCopyFallback) {
    realSource();
    request(2, {0}, 2);
    realSourceOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    expectSourceEvents({{WL_DATA_SOURCE_CANCELLED, 0}});
}

TEST_F(CDataDeviceDragTest, ModernSourceReceivesNegotiationDropAndFinish) {
    realSource();
    request(2, {COPY | MOVE}, 2);
    realSourceOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    EXPECT_FALSE(PROTO::data->dndActive());
    request(3);
    expectSourceEvents({{WL_DATA_SOURCE_ACTION, COPY}, {WL_DATA_SOURCE_DND_DROP_PERFORMED, 0}, {WL_DATA_SOURCE_DND_FINISHED, 0}});
}

TEST_F(CDataDeviceDragTest, LegacySourceUsesCopyWithoutVersionThreeEvents) {
    realSource(2);
    realSourceOffer();
    EXPECT_EQ(m_realSource->actions(), COPY);
    setActions(COPY | MOVE, MOVE);
    EXPECT_EQ(selectedAction(), COPY);
    accept();
    performDrop();
    request(3);
    expectSourceEvents({});
}

TEST_F(CDataDeviceDragTest, LegacyDestinationCompletesRealSourceOnDestroy) {
    realSource();
    request(2, {COPY | MOVE}, 2);
    realSourceOffer(2);
    performDrop();
    destroyOffer();
    expectSourceEvents({{WL_DATA_SOURCE_ACTION, COPY}, {WL_DATA_SOURCE_DND_DROP_PERFORMED, 0}, {WL_DATA_SOURCE_DND_FINISHED, 0}});
}

TEST_F(CDataDeviceDragTest, DestroyedSourceWithdrawsActiveOfferBeforeQueuedRequests) {
    realSource();
    request(2, {COPY}, 2);
    realSourceOffer();
    request(1, {}, 2); // wl_data_source.destroy
    EXPECT_TRUE(m_offer->m_dead);
    EXPECT_FALSE(PROTO::data->dndActive());
    setActions(COPY, COPY);
    request(3);
    expectClientAlive();
}

TEST_F(CDataDeviceDragTest, DestroyedSourceWithdrawsDroppedOfferBeforeQueuedFinish) {
    realSource();
    request(2, {COPY | ASK}, 2);
    realSourceOffer();
    setActions(COPY | ASK, ASK);
    accept();
    performDrop();
    request(1, {}, 2);
    EXPECT_TRUE(m_offer->m_dead);
    setActions(COPY, COPY);
    request(3);
    expectClientAlive();
}

TEST_F(CDataDeviceDragTest, RealAskSourceReceivesFinalActionImmediatelyBeforeFinish) {
    realSource();
    request(2, {ASK | COPY | MOVE}, 2);
    realSourceOffer();
    setActions(ASK | COPY | MOVE, ASK);
    accept();
    performDrop();
    setActions(MOVE, MOVE);
    expectSourceEvents({{WL_DATA_SOURCE_ACTION, ASK}, {WL_DATA_SOURCE_DND_DROP_PERFORMED, 0}});
    request(3);
    expectSourceEvents({{WL_DATA_SOURCE_ACTION, MOVE}, {WL_DATA_SOURCE_DND_FINISHED, 0}});
}

TEST_F(CDataDeviceDragTest, RealSourceGetsOneCancellationWhenDroppedOfferIsDestroyed) {
    realSource();
    request(2, {COPY}, 2);
    realSourceOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    destroyOffer();
    expectSourceEvents({{WL_DATA_SOURCE_ACTION, COPY}, {WL_DATA_SOURCE_DND_DROP_PERFORMED, 0}, {WL_DATA_SOURCE_CANCELLED, 0}});
}

TEST_F(CDataDeviceDragTest, UnresolvedAskStillRejectsFinishAfterInputIsReleased) {
    currentOffer();
    setActions(ASK, ASK);
    accept();
    performDrop();
    EXPECT_FALSE(PROTO::data->dndActive());
    request(3);
    expectError(WL_DATA_OFFER_ERROR_INVALID_FINISH);
    EXPECT_EQ(m_source->m_finished, 0);
}

TEST_F(CDataDeviceDragTest, AbortingNewDragDoesNotCancelDroppedTransfer) {
    currentOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    beginAnotherDrag();
    abortDrag();
    request(3);
    EXPECT_EQ(m_source->m_cancelled, 0);
    EXPECT_EQ(m_source->m_finished, 1);
    EXPECT_EQ(m_nextSource->m_cancelled, 1);
}

TEST_F(CDataDeviceDragTest, DestroyingDroppedSourceDoesNotAbortNewDrag) {
    realSource();
    request(2, {COPY}, 2);
    realSourceOffer();
    setActions(COPY, COPY);
    accept();
    performDrop();
    beginAnotherDrag();
    request(1, {}, 2);
    setActions(COPY, COPY);
    request(3);
    expectClientAlive();
    EXPECT_TRUE(PROTO::data->dndActive());
    EXPECT_EQ(m_nextSource->m_cancelled, 0);
    EXPECT_EQ(m_nextSource->m_finished, 0);
}

TEST_F(CDataDeviceDragTest, DroppedAskOfferCanStillTransferDataWithInputReleased) {
    currentOffer();
    setActions(COPY | ASK, ASK);
    accept();
    performDrop();
    ASSERT_FALSE(PROTO::data->dndActive());

    std::array<int, 2> sockets = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()), 0);
    CFileDescriptor sender{sockets[0]};
    CFileDescriptor receiver{sockets[1]};
    m_source->m_allowSend = true;
    std::vector<uint32_t> args(1 + (sizeof(MIME) + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
    args[0] = sizeof(MIME);
    std::memcpy(args.data() + 1, MIME, sizeof(MIME));
    request(1, args, 0, sender.get()); // wl_data_offer.receive
    EXPECT_EQ(m_source->m_sentMime, MIME);
    ASSERT_GE(m_source->m_sentFd.get(), 0);
    const uint32_t payload = 42;
    ASSERT_EQ(send(m_source->m_sentFd.get(), &payload, sizeof(payload), MSG_NOSIGNAL), sc<ssize_t>(sizeof(payload)));
    uint32_t received = 0;
    ASSERT_EQ(recv(receiver.get(), &received, sizeof(received), MSG_DONTWAIT), sc<ssize_t>(sizeof(received)));
    EXPECT_EQ(received, payload);
    EXPECT_EQ(m_source->m_finished, 0);
    setActions(COPY, COPY);
    request(3);
    EXPECT_EQ(m_source->m_finished, 1);
}
