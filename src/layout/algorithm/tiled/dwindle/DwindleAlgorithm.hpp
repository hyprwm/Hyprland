#include "../../TiledAlgorithm.hpp"

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Tiled {
    struct SDwindleNodeData {
        WP<SDwindleNodeData>                pParent;
        bool                                isNode = false;
        WP<ITarget>                         pTarget;
        std::array<WP<SDwindleNodeData>, 2> children = {};
        WP<SDwindleNodeData>                self;
        bool                                splitTop               = false; // for preserve_split
        CBox                                box                    = {0};
        float                               splitRatio             = 1.f;
        bool                                valid                  = true;
        bool                                ignoreFullscreenChecks = false;

        // For list lookup
        bool operator==(const SDwindleNodeData& rhs) const {
            return pTarget.lock() == rhs.pTarget.lock() && box == rhs.box && pParent == rhs.pParent && children[0] == rhs.children[0] && children[1] == rhs.children[1];
        }

        void recalcSizePosRecursive(bool force = false, bool horizontalOverride = false, bool verticalOverride = false);
    };

    class CDwindleAlgorithm : public ITiledAlgorithm {
      public:
        CDwindleAlgorithm()          = default;
        virtual ~CDwindleAlgorithm() = default;

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

        SP<SDwindleNodeData>                     getNodeFromWindow(PHLWINDOW w);

      private:
        std::vector<SP<SDwindleNodeData>> m_dwindleNodesData;

        struct {
            bool started = false;
            bool pseudo  = false;
            bool xExtent = false;
            bool yExtent = false;
        } m_pseudoDragFlags;

        std::optional<Vector2D> m_overrideFocalPoint; // for onWindowCreatedTiling.

        void                    addTarget(SP<ITarget> target);
        void                    calculateWorkspace();
        SP<SDwindleNodeData>    getNodeFromTarget(SP<ITarget>);
        int                     getNodes();
        SP<SDwindleNodeData>    getFirstNode();
        SP<SDwindleNodeData>    getClosestNode(const Vector2D&, SP<ITarget> skip = nullptr);
        SP<SDwindleNodeData>    getMasterNode();

        bool                    toggleSplit(SP<SDwindleNodeData>);
        bool                    swapSplit(SP<SDwindleNodeData>);
        void                    rotateSplit(SP<SDwindleNodeData>, int angle = 90);
        bool                    moveToRoot(SP<SDwindleNodeData>, bool stable = true);

        Math::eDirection        m_overrideDirection = Math::DIRECTION_DEFAULT;
    };
};
