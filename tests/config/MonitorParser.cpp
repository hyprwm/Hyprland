#include <config/shared/monitor/Parser.hpp>

#include <gtest/gtest.h>

using namespace Config;

TEST(Config, monitorParserName) {
    CMonitorRuleParser parser("DP-1");
    EXPECT_EQ(parser.name(), "DP-1");
    EXPECT_FALSE(parser.getError().has_value());
}

TEST(Config, monitorParserModeStandard) {
    CMonitorRuleParser parser("DP-1");
    EXPECT_TRUE(parser.parseMode("1920x1080"));
    EXPECT_EQ(parser.rule().m_resolution, Vector2D(1920, 1080));

    CMonitorRuleParser parser2("DP-2");
    EXPECT_TRUE(parser2.parseMode("2560x1440@144"));
    EXPECT_EQ(parser2.rule().m_resolution, Vector2D(2560, 1440));
    EXPECT_FLOAT_EQ(parser2.rule().m_refreshRate, 144.0f);

    CMonitorRuleParser parser3("DP-3");
    EXPECT_TRUE(parser3.parseMode("3840x2160@60.0"));
    EXPECT_EQ(parser3.rule().m_resolution, Vector2D(3840, 2160));
    EXPECT_FLOAT_EQ(parser3.rule().m_refreshRate, 60.0f);
}

TEST(Config, monitorParserModeKeywords) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parseMode("preferred"));
    EXPECT_EQ(p1.rule().m_resolution, Vector2D());

    CMonitorRuleParser p2("DP-1");
    EXPECT_TRUE(p2.parseMode("highrr"));
    EXPECT_EQ(p2.rule().m_resolution, Vector2D(-1, -1));

    CMonitorRuleParser p3("DP-1");
    EXPECT_TRUE(p3.parseMode("highres"));
    EXPECT_EQ(p3.rule().m_resolution, Vector2D(-1, -2));

    CMonitorRuleParser p4("DP-1");
    EXPECT_TRUE(p4.parseMode("maxwidth"));
    EXPECT_EQ(p4.rule().m_resolution, Vector2D(-1, -3));
}

TEST(Config, monitorParserModeInvalid) {
    CMonitorRuleParser parser("DP-1");
    EXPECT_FALSE(parser.parseMode("notaresolution"));
    EXPECT_TRUE(parser.getError().has_value());
}

TEST(Config, monitorParserPositionExplicit) {
    CMonitorRuleParser parser("DP-1");
    EXPECT_TRUE(parser.parsePosition("1920x0"));
    EXPECT_EQ(parser.rule().m_offset, Vector2D(1920, 0));

    CMonitorRuleParser parser2("DP-2");
    EXPECT_TRUE(parser2.parsePosition("0x1080"));
    EXPECT_EQ(parser2.rule().m_offset, Vector2D(0, 1080));
}

TEST(Config, monitorParserPositionAuto) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parsePosition("auto"));
    EXPECT_EQ(p1.rule().m_autoDir, DIR_AUTO_RIGHT);

    CMonitorRuleParser p2("DP-1");
    EXPECT_TRUE(p2.parsePosition("auto-left"));
    EXPECT_EQ(p2.rule().m_autoDir, DIR_AUTO_LEFT);

    CMonitorRuleParser p3("DP-1");
    EXPECT_TRUE(p3.parsePosition("auto-up"));
    EXPECT_EQ(p3.rule().m_autoDir, DIR_AUTO_UP);

    CMonitorRuleParser p4("DP-1");
    EXPECT_TRUE(p4.parsePosition("auto-down"));
    EXPECT_EQ(p4.rule().m_autoDir, DIR_AUTO_DOWN);

    CMonitorRuleParser p5("DP-1");
    EXPECT_TRUE(p5.parsePosition("auto-right"));
    EXPECT_EQ(p5.rule().m_autoDir, DIR_AUTO_RIGHT);
}

TEST(Config, monitorParserPositionAutoCenter) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parsePosition("auto-center-right"));
    EXPECT_EQ(p1.rule().m_autoDir, DIR_AUTO_CENTER_RIGHT);

    CMonitorRuleParser p2("DP-1");
    EXPECT_TRUE(p2.parsePosition("auto-center-left"));
    EXPECT_EQ(p2.rule().m_autoDir, DIR_AUTO_CENTER_LEFT);

    CMonitorRuleParser p3("DP-1");
    EXPECT_TRUE(p3.parsePosition("auto-center-up"));
    EXPECT_EQ(p3.rule().m_autoDir, DIR_AUTO_CENTER_UP);

    CMonitorRuleParser p4("DP-1");
    EXPECT_TRUE(p4.parsePosition("auto-center-down"));
    EXPECT_EQ(p4.rule().m_autoDir, DIR_AUTO_CENTER_DOWN);
}

TEST(Config, monitorParserScaleValid) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parseScale("1.5"));
    EXPECT_FLOAT_EQ(p1.rule().m_scale, 1.5f);

    CMonitorRuleParser p2("DP-1");
    EXPECT_TRUE(p2.parseScale("2"));
    EXPECT_FLOAT_EQ(p2.rule().m_scale, 2.0f);

    CMonitorRuleParser p3("DP-1");
    EXPECT_TRUE(p3.parseScale("auto"));
    EXPECT_FLOAT_EQ(p3.rule().m_scale, -1.0f);

    CMonitorRuleParser p4("DP-1");
    EXPECT_TRUE(p4.parseScale("0.25"));
    EXPECT_FLOAT_EQ(p4.rule().m_scale, 0.25f);
}

TEST(Config, monitorParserScaleInvalid) {
    CMonitorRuleParser parser("DP-1");
    EXPECT_FALSE(parser.parseScale("notanumber"));
    EXPECT_TRUE(parser.getError().has_value());
}

TEST(Config, monitorParserTransformValid) {
    for (int i = 0; i <= 7; i++) {
        CMonitorRuleParser parser("DP-1");
        EXPECT_TRUE(parser.parseTransform(std::to_string(i)));
        EXPECT_EQ(parser.rule().m_transform, sc<wl_output_transform>(i));
    }
}

TEST(Config, monitorParserTransformInvalid) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_FALSE(p1.parseTransform("8"));

    CMonitorRuleParser p2("DP-1");
    EXPECT_FALSE(p2.parseTransform("-1"));

    CMonitorRuleParser p3("DP-1");
    EXPECT_FALSE(p3.parseTransform("abc"));
}

TEST(Config, monitorParserBitdepth) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parseBitdepth("10"));
    EXPECT_TRUE(p1.rule().m_enable10bit);

    CMonitorRuleParser p2("DP-1");
    EXPECT_TRUE(p2.parseBitdepth("8"));
    EXPECT_FALSE(p2.rule().m_enable10bit);
}

TEST(Config, monitorParserCM) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parseCM("srgb"));
    EXPECT_EQ(p1.rule().m_cmType, NCMType::CM_SRGB);

    CMonitorRuleParser p2("DP-1");
    EXPECT_TRUE(p2.parseCM("wide"));
    EXPECT_EQ(p2.rule().m_cmType, NCMType::CM_WIDE);

    CMonitorRuleParser p3("DP-1");
    EXPECT_FALSE(p3.parseCM("invalid"));
    EXPECT_TRUE(p3.getError().has_value());
}

TEST(Config, monitorParserVRR) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parseVRR("1"));
    EXPECT_EQ(p1.rule().m_vrr, 1);

    CMonitorRuleParser p2("DP-1");
    EXPECT_TRUE(p2.parseVRR("0"));
    EXPECT_EQ(p2.rule().m_vrr, 0);

    CMonitorRuleParser p3("DP-1");
    EXPECT_FALSE(p3.parseVRR("abc"));
    EXPECT_TRUE(p3.getError().has_value());
}

TEST(Config, monitorParserSDRBrightness) {
    CMonitorRuleParser parser("DP-1");
    EXPECT_TRUE(parser.parseSDRBrightness("1.5"));
    EXPECT_FLOAT_EQ(parser.rule().m_sdrBrightness, 1.5f);

    CMonitorRuleParser p2("DP-1");
    EXPECT_FALSE(p2.parseSDRBrightness("notanumber"));
    EXPECT_TRUE(p2.getError().has_value());
}

TEST(Config, monitorParserICC) {
    CMonitorRuleParser p1("DP-1");
    EXPECT_TRUE(p1.parseICC("/path/to/profile.icc"));
    EXPECT_EQ(p1.rule().m_iccFile, "/path/to/profile.icc");

    CMonitorRuleParser p2("DP-1");
    EXPECT_FALSE(p2.parseICC(""));
    EXPECT_TRUE(p2.getError().has_value());
}

TEST(Config, monitorParserSetDisabled) {
    CMonitorRuleParser parser("DP-1");
    parser.setDisabled();
    EXPECT_TRUE(parser.rule().m_disabled);
}

TEST(Config, monitorParserSetMirror) {
    CMonitorRuleParser parser("DP-1");
    parser.setMirror("HDMI-A-1");
    EXPECT_EQ(parser.rule().m_mirrorOf, "HDMI-A-1");
}
