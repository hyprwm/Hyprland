#include <plugins/HookSystem.hpp>

#include <gtest/gtest.h>

#if defined(__aarch64__)

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

// Synthetic functions, one per relocation case. Written in asm so the displaced
// instructions (the first 16 or 20 bytes) are exactly what each test wants.
// All of them are int(int).
// clang-format off
#define TEST_FN(name) ".globl " #name "\n.type " #name ", %function\n.balign 4\n" #name ":\n"
asm(R"(
.text
)" TEST_FN(hookTestPlain) R"(
    add w0, w0, #1
    add w0, w0, #2
    add w0, w0, #3
    add w0, w0, #4
    ret
)" TEST_FN(hookTestAdr) R"(
    adr x1, hookTestData
    ldr x1, [x1]
    add w0, w0, w1
    nop
    ret
)" TEST_FN(hookTestAdrp) R"(
    adrp x1, hookTestData
    add x1, x1, :lo12:hookTestData
    ldr x1, [x1]
    add w0, w0, w1
    ret
)" TEST_FN(hookTestLdrX) R"(
    ldr x1, hookTestData
    add w0, w0, w1
    nop
    nop
    ret
)" TEST_FN(hookTestLdrW) R"(
    nop
    ldr w1, hookTestData
    add w0, w0, w1
    nop
    ret
)" TEST_FN(hookTestLdrsw) R"(
    nop
    nop
    ldrsw x1, hookTestData
    add w0, w0, w1
    ret
)" TEST_FN(hookTestLdrD) R"(
    ldr d0, hookTestData
    fmov x1, d0
    add w0, w0, w1
    nop
    ret
)" TEST_FN(hookTestLdrQ) R"(
    nop
    ldr q0, hookTestData
    umov x1, v0.d[1]
    add w0, w0, w1
    ret
)" TEST_FN(hookTestPrfm) R"(
    prfm pldl1keep, hookTestData
    add w0, w0, #5
    nop
    nop
    ret
)" TEST_FN(hookTestBcond) R"(
    cmp w0, #0
    b.lt 1f
    add w0, w0, #1
    ret
1:  sub w0, w0, #1
    ret
)" TEST_FN(hookTestCbz) R"(
    nop
    cbz w0, 1f
    add w0, w0, #1
    ret
1:  mov w0, #100
    ret
)" TEST_FN(hookTestTbz) R"(
    nop
    nop
    tbz w0, #0, 1f
    add w0, w0, #10
    ret
1:  add w0, w0, #20
    ret
)" TEST_FN(hookTestB) R"(
    add w0, w0, #1
    add w0, w0, #2
    add w0, w0, #3
    b 1f
    brk #0
1:  add w0, w0, #4
    ret
)" TEST_FN(hookTestBl) R"(
    stp x29, x30, [sp, #-16]!
    bl hookTestCallee
    add w0, w0, #1
    ldp x29, x30, [sp], #16
    ret
)" TEST_FN(hookTestCallee) R"(
    add w0, w0, #40
    ret
)" TEST_FN(hookTestBti) R"(
    bti c
    add w0, w0, #1
    add w0, w0, #2
    add w0, w0, #3
    add w0, w0, #4
    ret
)" TEST_FN(hookTestPaciasp) R"(
    paciasp
    add w0, w0, #1
    add w0, w0, #2
    add w0, w0, #3
    add w0, w0, #4
    autiasp
    ret
)" TEST_FN(hookTestTooShort) R"(
    add w0, w0, #7
    ret
)" TEST_FN(hookTestBranchNotLast) R"(
    add w0, w0, #1
    b 1f
    nop
    nop
1:  ret
.balign 16
hookTestData:
    .quad 42
    .quad 7
)");
// clang-format on

extern "C" int hookTestPlain(int);
extern "C" int hookTestAdr(int);
extern "C" int hookTestAdrp(int);
extern "C" int hookTestLdrX(int);
extern "C" int hookTestLdrW(int);
extern "C" int hookTestLdrsw(int);
extern "C" int hookTestLdrD(int);
extern "C" int hookTestLdrQ(int);
extern "C" int hookTestPrfm(int);
extern "C" int hookTestBcond(int);
extern "C" int hookTestCbz(int);
extern "C" int hookTestTbz(int);
extern "C" int hookTestB(int);
extern "C" int hookTestBl(int);
extern "C" int hookTestBti(int);
extern "C" int hookTestPaciasp(int);
extern "C" int hookTestTooShort(int);
extern "C" int hookTestBranchNotLast(int);

typedef int (*hookTestFn)(int);

static hookTestFn g_original = nullptr;
static int        g_hookRuns = 0;

static int        hookTestHook(int x) {
    g_hookRuns++;
    return g_original(x) + 1000;
}

class HookSystemAArch64 : public ::testing::Test {
  protected:
    void SetUp() override {
        if (!g_pFunctionHookSystem)
            g_pFunctionHookSystem = makeUnique<CHookSystem>();
    }

    virtual void afterHook() {
        ;
    }

    // hooks fn, checks the hook runs and the trampoline behaves like the original, then unhooks
    void checkHook(hookTestFn fn, const std::vector<int>& inputs) {
        std::vector<int> expected;
        for (const auto& IN : inputs) {
            expected.push_back(fn(IN));
        }

        auto* hook = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(fn), rc<void*>(hookTestHook));
        ASSERT_TRUE(hook->hook());
        ASSERT_NE(hook->m_original, nullptr);
        g_original = rc<hookTestFn>(hook->m_original);
        afterHook();

        for (size_t i = 0; i < inputs.size(); ++i) {
            g_hookRuns = 0;
            EXPECT_EQ(fn(inputs[i]), expected[i] + 1000);
            EXPECT_EQ(g_hookRuns, 1);
            EXPECT_EQ(g_original(inputs[i]), expected[i]);
        }

        ASSERT_TRUE(hook->unhook());
        g_original = nullptr;

        for (size_t i = 0; i < inputs.size(); ++i) {
            g_hookRuns = 0;
            EXPECT_EQ(fn(inputs[i]), expected[i]);
            EXPECT_EQ(g_hookRuns, 0);
        }

        g_pFunctionHookSystem->removeHook(hook);
    }
};

TEST_F(HookSystemAArch64, plain) {
    checkHook(hookTestPlain, {0, 5, -20});
}

TEST_F(HookSystemAArch64, adr) {
    checkHook(hookTestAdr, {0, 5});
}

TEST_F(HookSystemAArch64, adrp) {
    checkHook(hookTestAdrp, {0, 5});
}

TEST_F(HookSystemAArch64, ldrLiteralX) {
    checkHook(hookTestLdrX, {0, 5});
}

TEST_F(HookSystemAArch64, ldrLiteralW) {
    checkHook(hookTestLdrW, {0, 5});
}

TEST_F(HookSystemAArch64, ldrLiteralSW) {
    checkHook(hookTestLdrsw, {0, 5});
}

TEST_F(HookSystemAArch64, ldrLiteralD) {
    checkHook(hookTestLdrD, {0, 5});
}

TEST_F(HookSystemAArch64, ldrLiteralQ) {
    checkHook(hookTestLdrQ, {0, 5});
}

TEST_F(HookSystemAArch64, prfmLiteral) {
    checkHook(hookTestPrfm, {0, 5});
}

TEST_F(HookSystemAArch64, bCond) {
    checkHook(hookTestBcond, {5, -5});
}

TEST_F(HookSystemAArch64, cbz) {
    checkHook(hookTestCbz, {0, 5});
}

TEST_F(HookSystemAArch64, tbz) {
    checkHook(hookTestTbz, {2, 3});
}

TEST_F(HookSystemAArch64, b) {
    checkHook(hookTestB, {0, 5});
}

TEST_F(HookSystemAArch64, bl) {
    checkHook(hookTestBl, {0, 5});
}

TEST_F(HookSystemAArch64, btiEntry) {
    checkHook(hookTestBti, {0, 5});
    // the landing pad must survive the patch
    auto* hook = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(hookTestBti), rc<void*>(hookTestHook));
    ASSERT_TRUE(hook->hook());
    EXPECT_EQ(*rc<uint32_t*>(hookTestBti), 0xD503245Fu /* BTI c */);
    ASSERT_TRUE(hook->unhook());
    g_pFunctionHookSystem->removeHook(hook);
}

TEST_F(HookSystemAArch64, paciaspEntry) {
    checkHook(hookTestPaciasp, {0, 5});
    auto* hook = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(hookTestPaciasp), rc<void*>(hookTestHook));
    ASSERT_TRUE(hook->hook());
    EXPECT_EQ(*rc<uint32_t*>(hookTestPaciasp), 0xD503245Fu /* BTI c */);
    // PACIASP must have moved to the trampoline, not be executed before jumping to the hook
    EXPECT_EQ(*rc<uint32_t*>(hook->m_original), 0xD503233Fu /* PACIASP */);
    ASSERT_TRUE(hook->unhook());
    g_pFunctionHookSystem->removeHook(hook);
}

TEST_F(HookSystemAArch64, refusesTooShort) {
    auto* hook = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(hookTestTooShort), rc<void*>(hookTestHook));
    EXPECT_FALSE(hook->hook());
    EXPECT_EQ(hookTestTooShort(1), 8);
    g_pFunctionHookSystem->removeHook(hook);
}

TEST_F(HookSystemAArch64, refusesBranchNotLast) {
    auto* hook = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(hookTestBranchNotLast), rc<void*>(hookTestHook));
    EXPECT_FALSE(hook->hook());
    EXPECT_EQ(hookTestBranchNotLast(1), 2);
    g_pFunctionHookSystem->removeHook(hook);
}

TEST_F(HookSystemAArch64, refusesDoubleHook) {
    auto* hook = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(hookTestPlain), rc<void*>(hookTestHook));
    ASSERT_TRUE(hook->hook());
    auto* hook2 = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(hookTestPlain), rc<void*>(hookTestHook));
    EXPECT_FALSE(hook2->hook());
    ASSERT_TRUE(hook->unhook());
    g_pFunctionHookSystem->removeHook(hook2);
    g_pFunctionHookSystem->removeHook(hook);
}

#ifndef HWCAP2_BTI
#define HWCAP2_BTI (1 << 17)
#endif
#ifndef PROT_BTI
#define PROT_BTI 0x10
#endif

static bool btiEnforced() {
    return getauxval(AT_HWCAP2) & HWCAP2_BTI;
}

// Copies of test functions in a page far (> 128 MiB) from the trampoline pages, so the jumps out
// of the trampoline cannot be direct branches and go through X17. Where the CPU supports BTI (e.g.
// qemu-aarch64 -cpu max) the page is BTI guarded, so landing pads are enforced at the hooked entry,
// at the jump back from the trampoline and at the relocated PACIASP.
class HookSystemAArch64Far : public HookSystemAArch64 {
  protected:
    void SetUp() override {
        HookSystemAArch64::SetUp();

        const uint64_t PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
        const uint64_t HINT         = (rc<uint64_t>(hookTestPlain) + (1ULL << 30)) & ~(PAGESIZE_VAR - 1);
        m_page                      = mmap(rc<void*>(HINT), PAGESIZE_VAR, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        ASSERT_NE(m_page, MAP_FAILED);

        // find out where the trampolines live
        auto* probe = g_pFunctionHookSystem->initHook(nullptr, rc<void*>(farCopy(hookTestPlain, 7)), rc<void*>(hookTestHook));
        ASSERT_TRUE(probe->hook());
        const uint64_t TRAMPO = rc<uint64_t>(probe->m_original);
        ASSERT_TRUE(probe->unhook());
        g_pFunctionHookSystem->removeHook(probe);

        if (std::abs(sc<int64_t>(rc<uint64_t>(m_page) - TRAMPO)) < (1LL << 27))
            GTEST_SKIP() << "could not map a page far enough from the trampolines";
    }

    void TearDown() override {
        if (m_page != MAP_FAILED)
            munmap(m_page, sysconf(_SC_PAGE_SIZE));
    }

    void guard() {
        // /proc/self/smaps does not report BTI guarding under qemu-user, so re-guard after every write
        mprotect(m_page, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC | (m_guarded && btiEnforced() ? PROT_BTI : 0));
    }

    // copies 64 bytes of fn to the far page; fn must not use anything outside itself.
    // guarded functions must start with a landing pad, they are called indirectly.
    hookTestFn farCopy(hookTestFn fn, size_t slot, bool guarded = false) {
        auto* dst = sc<uint8_t*>(m_page) + slot * 64;
        m_guarded = guarded;
        mprotect(m_page, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);
        memcpy(dst, rc<const void*>(fn), 64);
        __builtin___clear_cache(rc<char*>(dst), rc<char*>(dst) + 64);
        guard();
        return rc<hookTestFn>(dst);
    }

    void afterHook() override {
        guard();
    }

    void* m_page    = MAP_FAILED;
    bool  m_guarded = false;
};

TEST_F(HookSystemAArch64Far, plain) {
    checkHook(farCopy(hookTestPlain, 0), {0, 5});
}

TEST_F(HookSystemAArch64Far, bCond) {
    checkHook(farCopy(hookTestBcond, 1), {5, -5});
}

TEST_F(HookSystemAArch64Far, tbz) {
    checkHook(farCopy(hookTestTbz, 2), {2, 3});
}

TEST_F(HookSystemAArch64Far, btiEntry) {
    checkHook(farCopy(hookTestBti, 3, true), {0, 5});
}

TEST_F(HookSystemAArch64Far, paciaspEntry) {
    checkHook(farCopy(hookTestPaciasp, 4, true), {0, 5});
}

// Negative control: only meaningful where BTI is enforced (BTI-capable CPU, e.g. qemu-aarch64 -cpu max).
// Overwriting the landing pad with the jump stub must fault on an indirect call, proving the
// enforcement the tests above run under is real.
TEST_F(HookSystemAArch64Far, btiEnforcedNegativeControl) {
    if (!btiEnforced())
        GTEST_SKIP() << "no BTI on this CPU";

    const auto     FN     = farCopy(hookTestBti, 5, true);
    const uint32_t STUB[] = {0x58000050, 0xD61F0200, sc<uint32_t>(rc<uint64_t>(hookTestPlain)), sc<uint32_t>(rc<uint64_t>(hookTestPlain) >> 32)};

    mprotect(m_page, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(rc<void*>(FN), STUB, sizeof(STUB));
    __builtin___clear_cache(rc<char*>(FN), rc<char*>(FN) + sizeof(STUB));
    guard();

    EXPECT_EXIT(exit(FN(1)), ::testing::KilledBySignal(SIGILL), "");
}

#endif
