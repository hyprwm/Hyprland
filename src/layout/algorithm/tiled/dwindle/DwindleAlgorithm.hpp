#include "../../TiledAlgorithm.hpp"

namespace Layout {
    class CAlgorithm;
}

namespace Layout::Tiled {
    struct SDwindleNodeData;

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

      private:
        std::vector<SP<SDwindleNodeData>> m_dwindleNodesData;

        struct {
            bool started = false;
            bool pseudo  = false;
            bool xExtent = false;
            bool yExtent = false;
        } m_pseudoDragFlags;

        std::optional<Vector2D> m_overrideFocalPoint; // for onWindowCreatedTiling.

        void                    addTarget(SP<ITarget> target, bool newTarget = true);
        void                    calculateWorkspace();
        SP<SDwindleNodeData>    getNodeFromTarget(SP<ITarget>);
        SP<SDwindleNodeData>    getNodeFromWindow(PHLWINDOW w);
        int                     getNodes();
        SP<SDwindleNodeData>    getFirstNode();
        SP<SDwindleNodeData>    getClosestNode(const Vector2D&);
        SP<SDwindleNodeData>    getMasterNode();

        void                    toggleSplit(SP<SDwindleNodeData>);
        void                    swapSplit(SP<SDwindleNodeData>);
        void                    moveToRoot(SP<SDwindleNodeData>, bool stable = true);

        Math::eDirection        m_overrideDirection = Math::DIRECTION_DEFAULT;
    };
};