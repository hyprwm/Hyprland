#pragma once

#include "../helpers/Vector2D.hpp"
#include "../Window.hpp"
#include "../helpers/Workspace.hpp"

#include <iostream>
#include <istream>
#include <ostream>
#include <algorithm>

#define STRAYS_WORKSPACE_NAME "strays"
#define STRAYS_WORKSPACE_ID   (INT_MAX - 1)

class necromancy_error : public std::runtime_error {
  public:
    explicit necromancy_error(const std::string&);
    explicit necromancy_error(const char*);

    // display as a notification, duration is in milliseconds
    void notify(const float duration = 10000) const;
    // log error in Hyprland log file
    necromancy_error& log();
};

// clangd, four fucking generations of inbreeding
struct SWindowState;
typedef std::unordered_map<uintptr_t, SWindowState> window_state_map_t;

namespace Necromancy {
    const std::string SIGNATURE = "HYPR NECROMANCY";
    const int         VERSION   = 1;

    /**
		Serialize data to binary stream
	*/
    template <typename T>
    void dump(std::ostream& os, const T& data) {
        os.write(reinterpret_cast<const char*>(&data), sizeof(data));
    }

    template <>
    void dump(std::ostream& os, const std::string& data);

    template <>
    void dump(std::ostream& os, const Vector2D& data);

    /**
		Deserialize from binary stream to data
	*/
    template <typename T>
    void load(std::istream& is, T& data) {
        is.read(reinterpret_cast<char*>(&data), sizeof(data));
    }

    template <>
    void load(std::istream& is, std::string& data);

    template <>
    void load(std::istream& is, Vector2D& data);

    // check if window is qualified for saving
    bool isWindowSavable(CWindow* pWindow);

    void createHeader(std::ostream& os);
    // return true is head is valid
    bool validateHeader(std::istream& is);

    /**
        @param location location of save file, use default location specified in config if it's empty
        @throw necromancy_error if layout cannot be written to filesystem.
    */
    void saveLayout(std::string location = "");
    /**
        @param location location of save file, use default location specified in config if it's empty
        @throw necromancy_error if layout save file cannot be read.
    */
    void restoreLayout(std::string location = "");
} // namespace Necromancy

// A snapshot of a window's woefully ephemeral mortal life
struct SWindowState {
    SWindowState() = default;
    SWindowState(CWindow*);

    CWindow* self = nullptr;

    // the following will be saved
    std::string cmdline = "";

    int         workspaceId = -1;
    int         monitorId   = -1;

    std::string title        = "";
    std::string class_       = "";
    std::string initialTitle = "";
    std::string initialClass = "";

    bool        isFloating       = false;
    bool        isFullscreen     = false;
    bool        isFakeFullscreen = false;
    bool        isPseudotiled    = false;
    bool        isPinned         = false;
    bool        isHidden         = false;

    // only used for floating window and pseudo
    Vector2D realPosition = {-1, -1};
    Vector2D realSize     = {-1, -1};

    struct GroupData {
        uintptr_t next   = 0;
        bool      head   = false;
        bool      locked = false;
    } groupData;

    /**
        Apply window state to window
    */
    void applyToWindow();
    /**
        Check if window state is valid (this->self != nullptr)
    */
    bool isValid();
    /**
        Match window
    */
    bool matchWindow(CWindow* pWindow);
    /**
        Collect all states for all window except for these in workspace for strays (orphanage)
    */
    static std::unique_ptr<window_state_map_t> collectAll();
    /**
        Dump all window states
    */
    static void dumpAll(std::ostream& os);
    /**
        Load all window states
    */
    static std::unique_ptr<window_state_map_t> loadAll(std::istream& is);
    /**
        Serialize window state into binary stream
    */
    void marshal(std::ostream& os);
    /**
        Fetch window state from binary stream
    */
    uintptr_t unmarshal(std::istream& is);

  private:
    void restoreSize();
    void apply(const auto&& callback);
};

struct SWorkspaceState {
    SWorkspaceState() = default;
    SWorkspaceState(CWorkspace*);
    int         id        = -1;
    std::string name      = "";
    uint64_t    monitorId = -1;

    struct SPrevWorkspaceData {
        int         id   = -1;
        std::string name = "";
    } prev;

    bool            isSpecial       = false;
    bool            defaultFloating = false;
    bool            defaultPseudo   = false;
    bool            immortal        = false;
    eFullscreenMode fullscreenMode  = FULLSCREEN_FULL;

    /**
        Apply state to workspace
    */
    void applyToWorkspace();
    /**
        Serialize workspace state into binary stream
    */
    void marshal(std::ostream& os);
    /**
        Fetch workspace state from binary stream
    */
    void unmarshal(std::istream& is);

  private:
    void apply(const auto&& callback);
};

// libc++ had a goddamn stroke with formatter specialisation
template <typename CharT>
struct std::formatter<SWindowState, CharT> : std::formatter<CharT> {
    bool use_json   = false;
    bool incomplete = false;
    FORMAT_PARSE(FORMAT_FLAG('j', use_json)    //
                 FORMAT_FLAG('I', incomplete), //
                 SWindowState)

    template <typename FormatContext>
    auto format(const SWindowState& ws, FormatContext& ctx) const {
        if (use_json)
            return std::format_to(ctx.out(), R"#(
      "hidden": {},
      "at": {:j},
      "size": {:j},
      "workspace": {},
      "floating": {},
      "class": "{}",
      "title": "{}",
      "initialClass": "{}",
      "initialTitle": "{}",
      "cmdline": "{}",
      "pinned": {},
      "fullscreen": {},
      "fakefullscreen": {},
      "pseudo": {})#",
                                  ws.isHidden, ws.realPosition, ws.realSize, ws.workspaceId, ws.isFloating, ws.class_, escapeJSONStrings(ws.title),
                                  escapeJSONStrings(ws.initialClass), escapeJSONStrings(ws.initialTitle), escapeJSONStrings(ws.cmdline), ws.isPinned, ws.isFullscreen,
                                  ws.isFakeFullscreen, ws.isPseudotiled);
        auto heading = incomplete ? "" : std::format("Window: {:x} -> {}:\n", (uintptr_t)ws.self, ws.title);
        auto group   = incomplete ? "" : std::format("\n\tgroupped:       next:{:x}, head:{}, locked: {}\n", ws.groupData.next, ws.groupData.head, ws.groupData.locked);
        return std::format_to( //
            ctx.out(),
            "{}"
            "\thidden:         {}\n"
            "\tat:             {}\n"
            "\tsize:           {}\n"
            "\tworkspace:      {}\n"
            "\tfloating:       {}\n"
            "\tclass:          {}\n"
            "\ttitle:          {}\n"
            "\tinitialClass:   {}\n"
            "\tinitialTitle:   {}\n"
            "\tcmdline:        {}\n"
            "\tpinned:         {}\n"
            "\tfullscreen:     {}\n"
            "\tfakefullscreen: {}\n"
            "\tpseudo:         {}"
            "{}",
            heading,             //
            ws.isHidden,         //
            ws.realPosition,     //
            ws.realSize,         //
            ws.workspaceId,      //
            ws.isFloating,       //
            ws.class_,           //
            ws.title,            //
            ws.initialClass,     //
            ws.initialTitle,     //
            ws.cmdline,          //
            ws.isPinned,         //
            ws.isFullscreen,     //
            ws.isFakeFullscreen, //
            ws.isPseudotiled,    //
            group);
    }
};

template <typename CharT>
struct std::formatter<window_state_map_t, CharT> : std::formatter<CharT> {
    bool use_json = false;
    FORMAT_PARSE(FORMAT_FLAG('j', use_json), window_state_map_t)

    template <typename FormatContext>
    auto format(const window_state_map_t& wsmap, FormatContext& ctx) const {
        std::ostringstream out;
        if (use_json)
            out << "[\n";

        size_t i = 0;
        for (auto& [addr, ws] : wsmap) {
            // window
            if (use_json) {
                out << std::format("    {{\n      \"address\": \"{:x}\",", addr);
                out << std::format("{:jI}", ws);
            } else {
                out << std::format("Window: {:x} -> {}:\n", addr, ws.title);
                out << std::format("{:I}\n", ws);
            }
            // group member
            if (ws.groupData.next != 0) {
                std::list<uintptr_t> members;
                uintptr_t            curr = addr;
                do {
                    members.push_back(curr);
                    curr = wsmap.at(curr).groupData.next;
                } while (curr != addr && curr != 0);

                if (use_json) {
                    out << std::format(",\n      \"grouped\": [\"{:x}\"", addr);
                    std::for_each(std::next(members.begin()), members.end(), [&](const uintptr_t& m) { out << std::format(", \"{:x}\"", m); });
                    out << "]";
                } else {
                    out << std::format("\tgrouped: {:x}", addr);
                    std::for_each(std::next(members.begin()), members.end(), [&](const uintptr_t& m) { out << std::format(", {:x}", m); });
                }
                out << "\n";
            }

            if (use_json)
                out << (i < wsmap.size() - 1 ? "    },\n" : "    }\n");
            else
                out << "\n";
            i++;
        }
        if (use_json)
            out << "  ]";
        return std::format_to(ctx.out(), "{}", out.str());
    }
};

template <typename CharT>
struct std::formatter<SWorkspaceState, CharT> : std::formatter<CharT> {
    bool use_json = false;
    FORMAT_PARSE(FORMAT_FLAG('j', use_json), SWorkspaceState);

    template <typename FormatContext>
    auto format(const SWorkspaceState& wss, FormatContext& ctx) const {
        if (use_json)
            return std::format_to( //
                ctx.out(),         //
                R"#(    {{
      "id": {},
      "name": "{}",
      "monitor": {},
      "defaultFloating": {},
      "defaultPseudo": {},
      "special": {}
    }})#",
                wss.id, escapeJSONStrings(wss.name), wss.monitorId, wss.defaultFloating, wss.defaultPseudo, wss.isSpecial);
        return std::format_to( //
            ctx.out(),
            "Workspace: {}\n"
            "\tname:            {}\n"
            "\tmonitor:         {}\n"
            "\tdefaultFloating: {}\n"
            "\tdefaultPseudo:   {}\n"
            "\tspecial:         {}",
            wss.id, wss.name, wss.monitorId, wss.defaultFloating, wss.defaultPseudo, wss.isSpecial);
    }
};