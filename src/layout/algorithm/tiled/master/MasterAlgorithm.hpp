#include "../../TiledAlgorithm.hpp"

#include <hyprutils/string/VarList2.hpp>

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Tiled {
    struct SMasterNodeData;

    //orientation determines which side of the screen the master area resides
    enum eOrientation : uint8_t {
        ORIENTATION_LEFT = 0,
        ORIENTATION_TOP,
        ORIENTATION_RIGHT,
        ORIENTATION_BOTTOM,
        ORIENTATION_CENTER
    };

    struct SMasterWorkspaceData {
        WORKSPACEID                 workspaceID = WORKSPACE_INVALID;
        std::optional<eOrientation> explicitOrientation;
        // Previously focused non-master window when `focusmaster previous` command was issued
        WP<ITarget> focusMasterPrev;

        //
        bool operator==(const SMasterWorkspaceData& rhs) const {
            return workspaceID == rhs.workspaceID;
        }
    };

    class CMasterAlgorithm : public ITiledAlgorithm {
      public:
        CMasterAlgorithm()          = default;
        virtual ~CMasterAlgorithm() = default;

        virtual void                             newTarget(SP<ITarget> target);
        virtual void                             movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void                             removeTarget(SP<ITarget> target);

        virtual void                             resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void                             recalculate();

        virtual SP<ITarget>                      getNextCandidate(SP<ITarget> old);

        virtual std::expected<void, std::string> layoutMsg(const std::string_view& sv);
        virtual std::optional<Vector2D>          predictSizeForNewTarget();

        virtual void                             swapTargets(SP<ITarget> a, SP<ITarget> b);
        virtual void                             moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

      private:
        std::vector<SP<SMasterNodeData>> m_masterNodesData;
        SMasterWorkspaceData             m_workspaceData;

        void                             addTarget(SP<ITarget> target, bool firstMap);

        bool                             m_forceWarps = false;

        void                             buildOrientationCycleVectorFromVars(std::vector<eOrientation>& cycle, Hyprutils::String::CVarList2* vars);
        void                             buildOrientationCycleVectorFromEOperation(std::vector<eOrientation>& cycle);
        void                             runOrientationCycle(Hyprutils::String::CVarList2* vars, int next);
        eOrientation                     getDynamicOrientation();
        int                              getNodesNo();
        SP<SMasterNodeData>              getNodeFromWindow(PHLWINDOW);
        SP<SMasterNodeData>              getNodeFromTarget(SP<ITarget>);
        SP<SMasterNodeData>              getMasterNode();
        SP<SMasterNodeData>              getClosestNode(const Vector2D&);
        void                             calculateWorkspace();
        SP<ITarget>                      getNextTarget(SP<ITarget>, bool, bool);
        int                              getMastersNo();
        bool                             isWindowTiled(PHLWINDOW);
        eOrientation                     defaultOrientation();
    };
};