#include <workspace/query/Query.hpp>

#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace Workspace;

class CQueryTestWorkspace final : public IAbstractWorkspace {
  public:
    CQueryTestWorkspace(WorkspaceID id, std::string address, eWorkspaceType type = eWorkspaceType::NORMAL) :
        IAbstractWorkspace(type), m_id(std::move(id)), m_address(std::move(address)) {
        ;
    }

    WorkspaceID id() const override {
        return m_id;
    }

    const std::string& displayName() const override {
        return m_address;
    }

    const std::string& addressableName() const override {
        return m_address;
    }

    SP<Monitor::IMonitorAddressable> monitor() const override {
        return {};
    }

  private:
    WorkspaceID m_id;
    std::string m_address;
};

static CQueryTestWorkspace numbered(uint32_t id) {
    return {SWorkspaceNumberedID{id}, std::to_string(id)};
}

static CQueryTestWorkspace addressed(WorkspaceID id, std::string address, eWorkspaceType type = eWorkspaceType::NORMAL) {
    return {std::move(id), std::move(address), type};
}

TEST(WorkspaceQuery, queryByNumberedID) {
    CQuery query;
    std::move(query).numbered(SWorkspaceNumberedID{2});
    const auto& QUERY = query;

    EXPECT_FALSE(QUERY.matches(numbered(1)));
    EXPECT_TRUE(QUERY.matches(numbered(2)));
}

TEST(WorkspaceQuery, selectorPreservesIdentityType) {
    const auto NUMBERED      = numbered(7);
    const auto NAMED_NUMBER  = addressed(SWorkspaceSpecialID{}, "7");
    const auto SPECIAL       = addressed(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::SPECIAL);
    const auto NAMED_SPECIAL = addressed(SWorkspaceSpecialID{}, "special:term");

    EXPECT_EQ(selector(NUMBERED), "7");
    EXPECT_EQ(selector(NAMED_NUMBER), "name:7");
    EXPECT_EQ(selector(SPECIAL), "special:term");
    EXPECT_EQ(selector(NAMED_SPECIAL), "name:special:term");
    EXPECT_EQ(identityTypeName(NUMBERED), "numbered");
    EXPECT_EQ(identityTypeName(NAMED_NUMBER), "named");
    EXPECT_EQ(identityTypeName(SPECIAL), "special");
    EXPECT_EQ(identityTypeName(NAMED_SPECIAL), "named");
}

TEST(WorkspaceQuery, queryByCanonicalAddress) {
    EXPECT_TRUE(std::move(CQuery{}).address("code").matches(addressed(SWorkspaceSpecialID{}, "code")));
    EXPECT_TRUE(std::move(CQuery{}).address("special:term").matches(addressed(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::SPECIAL)));
}

TEST(WorkspaceQuery, compatibilityInputNormalizesNamePrefix) {
    EXPECT_TRUE(std::move(CQuery{}).input("name:code").matches(addressed(SWorkspaceSpecialID{}, "code")));
    EXPECT_TRUE(std::move(CQuery{}).input("code").matches(addressed(SWorkspaceSpecialID{}, "code")));
    EXPECT_TRUE(std::move(CQuery{}).input("name:name:foo").matches(addressed(SWorkspaceSpecialID{}, "name:foo")));
    EXPECT_FALSE(std::move(CQuery{}).input("name:").matches(addressed(SWorkspaceSpecialID{}, "")));
}

TEST(WorkspaceQuery, numericInputUsesNumberedIdentity) {
    EXPECT_TRUE(std::move(CQuery{}).input("5").matches(numbered(5)));
    EXPECT_FALSE(std::move(CQuery{}).input("0").matches(numbered(0)));
}

TEST(WorkspaceQuery, specialCompatibilityInputUsesCanonicalAddress) {
    EXPECT_TRUE(std::move(CQuery{}).input("special").matches(addressed(SWorkspaceSpecialID{}, "special:special", eWorkspaceType::SPECIAL)));
    EXPECT_TRUE(std::move(CQuery{}).input("special:term").matches(addressed(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::SPECIAL)));
    EXPECT_FALSE(std::move(CQuery{}).input("special:").matches(addressed(SWorkspaceSpecialID{}, "special:", eWorkspaceType::SPECIAL)));
}

TEST(WorkspaceQuery, explicitInputsOnlyMatchTheirIdentityType) {
    const auto NUMBERED  = addressed(SWorkspaceNumberedID{1}, "1");
    const auto NAMED     = addressed(SWorkspaceSpecialID{}, "1");
    const auto SPECIAL   = addressed(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::SPECIAL);
    const auto COLLISION = addressed(SWorkspaceSpecialID{}, "special:term");

    EXPECT_TRUE(std::move(CQuery{}).input("1").matches(NUMBERED));
    EXPECT_FALSE(std::move(CQuery{}).input("1").matches(NAMED));
    EXPECT_TRUE(std::move(CQuery{}).input("name:1").matches(NAMED));
    EXPECT_FALSE(std::move(CQuery{}).input("name:1").matches(NUMBERED));
    EXPECT_TRUE(std::move(CQuery{}).input("special:term").matches(SPECIAL));
    EXPECT_FALSE(std::move(CQuery{}).input("special:term").matches(COLLISION));
    EXPECT_TRUE(std::move(CQuery{}).input("name:special:term").matches(COLLISION));
}

TEST(WorkspaceQuery, canonicalAddressLookupCannotStealTypedSyntax) {
    const auto NAMED_NUMBER  = addressed(SWorkspaceSpecialID{}, "7");
    const auto NUMBERED      = addressed(SWorkspaceNumberedID{7}, "7");
    const auto NAMED_SPECIAL = addressed(SWorkspaceSpecialID{}, "special:term");
    const auto SPECIAL       = addressed(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::SPECIAL);

    EXPECT_FALSE(std::move(CQuery{}).address("7").matches(NAMED_NUMBER));
    EXPECT_TRUE(std::move(CQuery{}).address("7").matches(NUMBERED));
    EXPECT_FALSE(std::move(CQuery{}).address("special:term").matches(NAMED_SPECIAL));
    EXPECT_TRUE(std::move(CQuery{}).address("special:term").matches(SPECIAL));
    EXPECT_TRUE(std::move(CQuery{}).identity(SWorkspaceSpecialID{}, "7", eWorkspaceType::NORMAL).matches(NAMED_NUMBER));
    EXPECT_TRUE(std::move(CQuery{}).identity(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::NORMAL).matches(NAMED_SPECIAL));
    EXPECT_FALSE(std::move(CQuery{}).identity(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::NORMAL).matches(SPECIAL));
    EXPECT_FALSE(std::move(CQuery{}).identity(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::SPECIAL).matches(NAMED_SPECIAL));
}

TEST(WorkspaceQuery, overflowingNumericInputDoesNotBecomeNamedInput) {
    EXPECT_FALSE(std::move(CQuery{}).input("999999999999999999999999").matches(addressed(SWorkspaceSpecialID{}, "999999999999999999999999")));
}

TEST(WorkspaceQuery, noCriteriaMatchesAnyWorkspace) {
    const CQuery QUERY;

    EXPECT_TRUE(QUERY.matches(numbered(1)));
    EXPECT_TRUE(QUERY.matches(addressed(SWorkspaceSpecialID{}, "code")));
}

TEST(WorkspaceQuery, ownsInputAndIdentityAddressStrings) {
    CQuery      inputQuery;
    CQuery      addressQuery;
    CQuery      identityQuery;
    std::string input           = "name:code";
    std::string address         = "special:term";
    std::string identityAddress = "special:other";
    std::move(inputQuery).input(input);
    std::move(addressQuery).address(address);
    std::move(identityQuery).identity(SWorkspaceSpecialID{}, identityAddress, eWorkspaceType::SPECIAL);

    input.assign("name:changed");
    address.assign("special:changed");
    identityAddress.assign("special:changed");

    EXPECT_TRUE(inputQuery.matches(addressed(SWorkspaceSpecialID{}, "code")));
    EXPECT_TRUE(addressQuery.matches(addressed(SWorkspaceSpecialID{}, "special:term", eWorkspaceType::SPECIAL)));
    EXPECT_TRUE(identityQuery.matches(addressed(SWorkspaceSpecialID{}, "special:other", eWorkspaceType::SPECIAL)));
}
