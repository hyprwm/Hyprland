#include <render/transformer/TransformerList.hpp>

#include <gtest/gtest.h>

class CTestDamageTransformer : public Render::IWindowTransformer {
  public:
    CTestDamageTransformer(int priority, double scale, double add, bool active = true) : m_priority(priority), m_scale(scale), m_add(add), m_active(active) {
        ;
    }

    virtual SP<Render::IFramebuffer> transform(SP<Render::IFramebuffer> in, const Render::SWindowTransformContext& context) {
        (void)context;
        return in;
    }

    virtual int priority() const {
        return m_priority;
    }

    virtual bool active() const {
        return m_active;
    }

    virtual CBox transformBoxForDamage(const CBox& currentBox) const {
        CBox box = currentBox;
        box.x    = box.x * m_scale + m_add;
        box.w *= m_scale;
        return box;
    }

  private:
    int    m_priority = 0;
    double m_scale    = 1.0;
    double m_add      = 0.0;
    bool   m_active   = true;
};

TEST(Render, transformerListDamageBoxFollowsPriorityOrder) {
    Render::CWindowTransformerList list;
    list.emplace<CTestDamageTransformer>(20, 2.0, 0.0);
    list.emplace<CTestDamageTransformer>(10, 1.0, 3.0);

    const CBox BOX = {1, 2, 10, 20};
    const auto OUT = list.transformBoxForDamage(BOX);

    EXPECT_DOUBLE_EQ(OUT.x, 8.0);
    EXPECT_DOUBLE_EQ(OUT.y, 2.0);
    EXPECT_DOUBLE_EQ(OUT.w, 20.0);
    EXPECT_DOUBLE_EQ(OUT.h, 20.0);
}

TEST(Render, transformerListDamageBoxSkipsInactiveTransformers) {
    Render::CWindowTransformerList list;
    list.emplace<CTestDamageTransformer>(10, 2.0, 0.0, false);
    list.emplace<CTestDamageTransformer>(20, 1.0, 3.0);

    const CBox BOX = {1, 2, 10, 20};
    const auto OUT = list.transformBoxForDamage(BOX);

    EXPECT_DOUBLE_EQ(OUT.x, 4.0);
    EXPECT_DOUBLE_EQ(OUT.w, 10.0);
}
