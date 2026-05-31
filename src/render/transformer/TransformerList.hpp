#pragma once

#include "Transformer.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace Render {
    class CWindowTransformerList {
      public:
        template <typename T, typename... Args>
        T* emplace(Args&&... args) {
            auto transformer = makeUnique<T>(std::forward<Args>(args)...);
            auto ptr         = transformer.get();
            m_transformers.emplace_back(std::move(transformer));
            sort();
            return ptr;
        }

        template <typename T>
        T* get() {
            for (auto const& transformer : m_transformers) {
                if (const auto TYPED = dc<T*>(transformer.get()))
                    return TYPED;
            }

            return nullptr;
        }

        template <typename T>
        const T* get() const {
            for (auto const& transformer : m_transformers) {
                if (const auto TYPED = dc<const T*>(transformer.get()))
                    return TYPED;
            }

            return nullptr;
        }

        bool                     empty() const;
        bool                     blocksDirectScanout() const;
        CBox                     transformedExtents(const CBox& currentBox) const;
        CBox                     sourceBoxForRender(const CBox& currentBox, const CBox& monitorBox) const;
        CBox                     transformBoxForDamage(const CBox& currentBox) const;

        void                     preWindowRender(CSurfacePassElement::SRenderData* pRenderData) const;
        void                     amendTransformedRenderData(const CBox& currentBox, SMotionBlurData* pMotionBlurData) const;
        SP<Render::IFramebuffer> transform(SP<Render::IFramebuffer> in, const SWindowTransformContext& context) const;

        void                     removeInactive();

      private:
        void                                        sort();

        std::vector<UP<Render::IWindowTransformer>> m_transformers;
    };
}
