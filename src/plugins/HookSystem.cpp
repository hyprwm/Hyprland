#include "HookSystem.hpp"
#include "../debug/log/Logger.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "../managers/TokenManager.hpp"
#include "../helpers/MiscFunctions.hpp"

#define register
#include <udis86.h>
#undef register
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

#include <hyprutils/string/Numeric.hpp>

using namespace Hyprutils::String;

CFunctionHook::CFunctionHook(HANDLE owner, void* source, void* destination) : m_source(source), m_destination(destination), m_owner(owner) {
    ;
}

CFunctionHook::~CFunctionHook() {
    if (m_active)
        unhook();
}

CFunctionHook::SInstructionProbe CFunctionHook::getInstructionLenAt(void* start) {
    ud_t udis;

    ud_init(&udis);
    ud_set_mode(&udis, 64);
    ud_set_syntax(&udis, UD_SYN_ATT);

    size_t curOffset = 1;
    size_t insSize   = 0;
    while (true) {
        ud_set_input_buffer(&udis, sc<uint8_t*>(start), curOffset);
        insSize = ud_disassemble(&udis);
        if (insSize != curOffset)
            break;
        curOffset++;
    }

    // check for RIP refs
    std::string ins;
    if (const auto CINS = ud_insn_asm(&udis); CINS)
        ins = std::string(CINS);

    return {insSize, ins};
}

CFunctionHook::SInstructionProbe CFunctionHook::probeMinimumJumpSize(void* start, size_t min) {

    size_t              size = 0;

    std::string         instrs = "";
    std::vector<size_t> sizes;

    while (size <= min) {
        // find info about this instruction
        auto probe = getInstructionLenAt(sc<uint8_t*>(start) + size);
        sizes.push_back(probe.len);
        size += probe.len;
        instrs += std::format("{}\n", probe.assembly);
    }

    return {size, instrs, sizes};
}

CFunctionHook::SAssembly CFunctionHook::fixInstructionProbeRIPCalls(const SInstructionProbe& probe) {
    SAssembly returns;

    // analyze the code and fix what we know how to.
    uint64_t currentAddress = rc<uint64_t>(m_source);
    // actually newline + 1
    size_t lastAsmNewline = 0;
    // needle for destination binary
    size_t            currentDestinationOffset = 0;

    std::vector<char> finalBytes;
    finalBytes.resize(probe.len);

    for (auto const& len : probe.insSizes) {

        // copy original bytes to our finalBytes
        for (size_t i = 0; i < len; ++i) {
            finalBytes[currentDestinationOffset + i] = *rc<char*>(currentAddress + i);
        }

        std::string code = probe.assembly.substr(lastAsmNewline, probe.assembly.find('\n', lastAsmNewline) - lastAsmNewline);
        if (code.contains("%rip")) {
            CVarList    tokens{code, 0, 's'};
            size_t      plusPresent  = tokens[1][0] == '+' ? 1 : 0;
            size_t      minusPresent = tokens[1][0] == '-' ? 1 : 0;
            std::string addr         = tokens[1].substr((plusPresent || minusPresent), tokens[1].find("(%rip)") - (plusPresent || minusPresent));
            auto        addrResult   = strToNumber<int64_t>(addr);
            if (!addrResult)
                return {};
            const int32_t OFFSET = (minusPresent ? -1 : 1) * *addrResult;
            if (OFFSET == 0)
                return {};
            const uint64_t DESTINATION = currentAddress + OFFSET + len;

            auto           ADDREND   = code.find("(%rip)");
            auto           ADDRSTART = (code.substr(0, ADDREND).find_last_of(' '));

            if (ADDREND == std::string::npos || ADDRSTART == std::string::npos)
                return {};

            const uint64_t PREDICTEDRIP = rc<uint64_t>(m_landTrampolineAddr) + currentDestinationOffset + len;
            const int32_t  NEWRIPOFFSET = DESTINATION - PREDICTEDRIP;

            size_t         ripOffset = 0;

            // find %rip usage offset from beginning
            for (int i = len - 4 /* 32-bit */; i > 0; --i) {
                if (*rc<int32_t*>(currentAddress + i) == OFFSET) {
                    ripOffset = i;
                    break;
                }
            }

            if (ripOffset == 0)
                return {};

            // fix offset in the final bytes. This doesn't care about endianness
            *rc<int32_t*>(&finalBytes[currentDestinationOffset + ripOffset]) = NEWRIPOFFSET;

            currentDestinationOffset += len;
        } else {
            currentDestinationOffset += len;
        }

        lastAsmNewline = probe.assembly.find('\n', lastAsmNewline) + 1;
        currentAddress += len;
    }

    return {finalBytes};
}

#if defined(__aarch64__)

/*
    AArch64 function hooks.

    Entry patch (16 bytes, displaces 4 instructions):
        LDR X16, #8
        BR  X16
        .quad <hook destination>
    X16 / X17 are IP0 / IP1: the ABI allows veneers to clobber them between a call and the
    callee's entry, so clobbering them here is legal at any function entry.

    If the function starts with a landing pad (BTI c/j/jc, or PACIASP / PACIBSP which act as an
    implicit BTI c), the patch is placed after a BTI c instead (20 bytes, 5 displaced instructions),
    so indirect calls into the hooked entry still land on a valid pad on BTI-enforcing systems.
    The hook destination is entered with BR X16, which is accepted by BTI c. The original landing
    pad (including a PACIASP) is relocated into the trampoline verbatim: it must only execute on the
    path that runs the original function, otherwise LR would be signed twice and the hook's return
    would fail authentication.

    The trampoline (what the plugin calls as "original") runs the displaced instructions, with all
    PC-relative ones rewritten (see relocateAArch64), and then jumps to entry + patch size. Jumps
    out of the trampoline are direct (B / BL) when within +-128 MiB, which is the common case as the
    trampoline pages are allocated next to the Hyprland mapping. Otherwise the target goes through
    X17: BLR X17 for calls (the target is a function entry, a valid landing pad) and RET X17 for
    jumps (unlike BR, RET is not subject to BTI checks, and the target is in the middle of the
    original function where no landing pad exists).
*/

static constexpr uint32_t AARCH64_NOP   = 0xD503201F;
static constexpr uint32_t AARCH64_BTI_C = 0xD503245F;

static bool               isAArch64BTI(uint32_t insn) {
    return (insn & 0xFFFFFF3F) == 0xD503241F; // BTI {c,j,jc}
}

static bool isAArch64LandingPad(uint32_t insn) {
    return isAArch64BTI(insn) || insn == 0xD503233F /* PACIASP */ || insn == 0xD503237F /* PACIBSP */;
}

static int64_t signExtend(uint64_t value, unsigned bits) {
    const uint64_t SIGN = 1ULL << (bits - 1);
    return sc<int64_t>((value ^ SIGN) - SIGN);
}

#ifndef PROT_BTI
#define PROT_BTI 0x10
#endif

// whether the mapping containing addr has BTI guarded pages ("bt" in VmFlags of /proc/self/smaps).
// mprotect() without PROT_BTI would silently drop the guard, so it has to be re-applied.
static bool isAArch64BTIGuarded(uint64_t addr) {
    auto        SMAPS   = std::ifstream("/proc/self/smaps");
    bool        inRange = false;
    std::string line;

    while (std::getline(SMAPS, line)) {
        if (line.starts_with("VmFlags:")) {
            if (inRange)
                return line.contains(" bt");
            continue;
        }

        // mapping header: start-end perms offset dev inode path
        const auto DASH  = line.find('-');
        const auto SPACE = line.find(' ');
        if (DASH == std::string::npos || SPACE == std::string::npos || DASH > SPACE)
            continue;

        try {
            const auto START = std::stoull(line.substr(0, DASH), nullptr, 16);
            const auto END   = std::stoull(line.substr(DASH + 1, SPACE - DASH - 1), nullptr, 16);
            inRange          = addr >= START && addr < END;
        } catch (std::exception& e) { inRange = false; }
    }

    return false;
}

// writes len bytes of code to dst, restoring the mapping's protection (including BTI guarding) afterwards
static void writeCodeAArch64(void* dst, const void* src, size_t len) {
    const uint64_t PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
    const uint64_t START        = rc<uint64_t>(dst) & ~(PAGESIZE_VAR - 1);
    const uint64_t END          = (rc<uint64_t>(dst) + len + PAGESIZE_VAR - 1) & ~(PAGESIZE_VAR - 1);
    const int      PROT         = PROT_READ | PROT_EXEC | (isAArch64BTIGuarded(rc<uint64_t>(dst)) ? PROT_BTI : 0);

    mprotect(rc<void*>(START), END - START, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(dst, src, len);
    mprotect(rc<void*>(START), END - START, PROT);
    __builtin___clear_cache(sc<char*>(dst), sc<char*>(dst) + len);
}

struct SAArch64Trampoline {
    uint64_t                               base = 0; // address the code will run at
    std::vector<uint32_t>                  code;
    std::vector<uint64_t>                  literals;
    std::vector<std::pair<size_t, size_t>> literalRefs; // code index -> literal index

    uint64_t                               pc() const {
        return base + code.size() * 4;
    }

    void emit(uint32_t insn) {
        code.push_back(insn);
    }

    // LDR <Xt|Wt|St|Dt|Qt>, <literal>, placed after the code. opcode must have imm19 == 0
    void emitLoadLiteral(uint32_t opcode, uint64_t value) {
        literalRefs.emplace_back(code.size(), literals.size());
        literals.push_back(value);
        code.push_back(opcode);
    }

    static bool inBranchRange(uint64_t from, uint64_t target) {
        const int64_t DELTA = sc<int64_t>(target - from);
        return DELTA >= -(1LL << 27) && DELTA < (1LL << 27);
    }

    void emitJump(uint64_t target, bool link) {
        if (inBranchRange(pc(), target)) {
            emit((link ? 0x94000000 : 0x14000000) | ((sc<uint64_t>(target - pc()) >> 2) & 0x03FFFFFF)); // B / BL target
            return;
        }

        emitLoadLiteral(0x58000011, target);  // LDR X17, <literal>
        emit(link ? 0xD63F0220 : 0xD65F0220); // BLR X17 / RET X17
    }

    // <cond branch> +8 ; B over ; <jump to target>
    void emitConditionalJump(uint32_t branchToPlus8, uint64_t target) {
        const uint32_t JUMPWORDS = inBranchRange(pc() + 8, target) ? 1 : 2;
        emit(branchToPlus8);
        emit(0x14000000 | (JUMPWORDS + 1));
        emitJump(target, false);
    }

    std::vector<uint8_t> finalize() {
        if (code.size() % 2)
            emit(AARCH64_NOP); // 8-byte align the literal pool

        for (const auto& [CODEIDX, LITIDX] : literalRefs) {
            code[CODEIDX] |= ((code.size() + LITIDX * 2 - CODEIDX) & 0x7FFFF) << 5;
        }

        std::vector<uint8_t> bytes(code.size() * 4 + literals.size() * 8);
        memcpy(bytes.data(), code.data(), code.size() * 4);
        memcpy(bytes.data() + code.size() * 4, literals.data(), literals.size() * 8);
        return bytes;
    }
};

/*
    Builds the trampoline for the first `count` instructions at `src`, resuming the original at
    src + count * 4 afterwards. Rewritten instructions:
        ADR / ADRP Xd, <label>      -> LDR Xd, =<address>
        LDR Wt / Xt / (LDRSW) <lit> -> LDR Xt, =<address> ; LDR Wt / Xt / LDRSW Xt, [Xt]
        LDR St / Dt / Qt, <lit>     -> LDR X17, =<address> ; LDR St / Dt / Qt, [X17]
        PRFM <lit>                  -> NOP
        B / BL <label>              -> direct B / BL, or via X17
        B.cond / CBZ / CBNZ / TBZ / TBNZ <label> -> <same> +8 ; B over ; jump to <label>
    Everything else is copied verbatim. Returns empty if:
        - an unconditional branch (B, BR, RET, ERET, ...) is displaced but is not the last displaced
          instruction: the function is shorter than the patch and the remaining bytes belong to
          something else,
        - an unallocated literal load is encountered.
    Like on x86, a branch from later in the function back into the patched bytes cannot be detected.
*/
static std::vector<uint8_t> relocateAArch64(const uint32_t* src, size_t count, uint64_t trampolineAddr) {
    SAArch64Trampoline t{.base = trampolineAddr};

    for (size_t i = 0; i < count; ++i) {
        const uint32_t INSN = src[i];
        const uint64_t PC   = rc<uint64_t>(src) + i * 4;
        const bool     LAST = i + 1 == count;

        if ((INSN & 0x9F000000) == 0x10000000) { // ADR
            const uint64_t IMM = ((INSN >> 29) & 3) | (((INSN >> 5) & 0x7FFFF) << 2);
            t.emitLoadLiteral(0x58000000 | (INSN & 0x1F), PC + signExtend(IMM, 21));
        } else if ((INSN & 0x9F000000) == 0x90000000) { // ADRP
            const uint64_t IMM = ((INSN >> 29) & 3) | (((INSN >> 5) & 0x7FFFF) << 2);
            t.emitLoadLiteral(0x58000000 | (INSN & 0x1F), (PC & ~0xFFFULL) + (signExtend(IMM, 21) << 12));
        } else if ((INSN & 0x3B000000) == 0x18000000) { // LDR (literal), LDRSW (literal), PRFM (literal)
            const uint64_t TARGET = PC + signExtend((INSN >> 5) & 0x7FFFF, 19) * 4;
            const uint32_t OPC    = INSN >> 30;
            const uint32_t RT     = INSN & 0x1F;

            if (INSN & (1 << 26)) { // SIMD&FP
                if (OPC == 3)
                    return {}; // unallocated

                static constexpr uint32_t LOADS[] = {0xBD400000 /* LDR St */, 0xFD400000 /* LDR Dt */, 0x3DC00000 /* LDR Qt */};
                t.emitLoadLiteral(0x58000011, TARGET); // LDR X17, =TARGET
                t.emit(LOADS[OPC] | (17 << 5) | RT);   // LDR Vt, [X17]
            } else if (OPC == 3) {
                t.emit(AARCH64_NOP); // PRFM is a hint, dropping it is harmless
            } else {
                static constexpr uint32_t LOADS[] = {0xB9400000 /* LDR Wt */, 0xF9400000 /* LDR Xt */, 0xB9800000 /* LDRSW Xt */};
                t.emitLoadLiteral(0x58000000 | RT, TARGET); // LDR Xt, =TARGET
                t.emit(LOADS[OPC] | (RT << 5) | RT);        // LDR Wt / Xt, [Xt]
            }
        } else if ((INSN & 0x7C000000) == 0x14000000) { // B / BL
            const bool LINK = INSN & 0x80000000;
            if (!LINK && !LAST)
                return {};
            t.emitJump(PC + signExtend(INSN & 0x03FFFFFF, 26) * 4, LINK);
        } else if ((INSN & 0xFF000000) == 0x54000000 || (INSN & 0x7E000000) == 0x34000000) { // B.cond / BC.cond, CBZ / CBNZ
            t.emitConditionalJump((INSN & ~(0x7FFFFu << 5)) | (2 << 5), PC + signExtend((INSN >> 5) & 0x7FFFF, 19) * 4);
        } else if ((INSN & 0x7E000000) == 0x36000000) { // TBZ / TBNZ
            t.emitConditionalJump((INSN & ~(0x3FFFu << 5)) | (2 << 5), PC + signExtend((INSN >> 5) & 0x3FFF, 14) * 4);
        } else if ((INSN & 0xFE000000) == 0xD6000000 && ((INSN >> 21) & 7) != 1) { // BR, RET, ERET, ... but not BLR / BLRAA / BLRAB
            if (!LAST)
                return {};
            t.emit(INSN);
        } else
            t.emit(INSN);
    }

    t.emitJump(rc<uint64_t>(src) + count * 4, false);

    return t.finalize();
}

bool CFunctionHook::hookAArch64() {
    if (g_pFunctionHookSystem->m_activeHooks.contains(rc<uint64_t>(m_source))) {
        LOG(Log::ERR, "[functionhook] failed, function is already hooked");
        return false;
    }

    if (rc<uint64_t>(m_source) % 4 != 0) {
        LOG(Log::ERR, "[functionhook] failed, source {:p} is not instruction aligned", m_source);
        return false;
    }

    const auto   SRC        = rc<const uint32_t*>(m_source);
    const bool   LANDINGPAD = isAArch64LandingPad(SRC[0]);
    const size_t PATCHWORDS = LANDINGPAD ? 5 : 4;

    m_landTrampolineAddr = rc<void*>(g_pFunctionHookSystem->getAddressForTrampo());

    const auto TRAMPOLINE = relocateAArch64(SRC, PATCHWORDS, rc<uint64_t>(m_landTrampolineAddr));

    if (TRAMPOLINE.empty()) {
        LOG(Log::ERR, "[functionhook] failed, cannot relocate the first {} instructions of {:p}: {:#010x} {:#010x} {:#010x} {:#010x} {:#010x}", PATCHWORDS, m_source, SRC[0],
            SRC[1], SRC[2], SRC[3], LANDINGPAD ? SRC[4] : 0);
        return false;
    }

    if (TRAMPOLINE.size() > HOOK_TRAMPOLINE_MAX_SIZE) {
        LOG(Log::ERR, "[functionhook] failed, not enough space in trampo to alloc: {} bytes", TRAMPOLINE.size());
        return false;
    }

    const size_t ORIGSIZE = PATCHWORDS * 4;
    m_originalBytes.resize(ORIGSIZE);
    memcpy(m_originalBytes.data(), m_source, ORIGSIZE);

    memcpy(m_landTrampolineAddr, TRAMPOLINE.data(), TRAMPOLINE.size());
    __builtin___clear_cache(sc<char*>(m_landTrampolineAddr), sc<char*>(m_landTrampolineAddr) + TRAMPOLINE.size());

    std::vector<uint32_t> patch;
    if (LANDINGPAD)
        patch.push_back(isAArch64BTI(SRC[0]) ? SRC[0] : AARCH64_BTI_C);
    patch.push_back(0x58000050); // LDR X16, #8
    patch.push_back(0xD61F0200); // BR X16
    patch.push_back(rc<uint64_t>(m_destination) & 0xFFFFFFFF);
    patch.push_back(rc<uint64_t>(m_destination) >> 32);

    writeCodeAArch64(m_source, patch.data(), ORIGSIZE);

    m_original = m_landTrampolineAddr;
    m_active   = true;
    m_hookLen  = ORIGSIZE;

    g_pFunctionHookSystem->m_activeHooks.emplace(rc<uint64_t>(m_source));

    LOG(Log::DEBUG, "[functionhook] hooked {:p} ({} bytes{}), trampoline at {:p} ({} bytes)", m_source, ORIGSIZE, LANDINGPAD ? ", landing pad kept" : "", m_landTrampolineAddr,
        TRAMPOLINE.size());

    return true;
}
#endif

bool CFunctionHook::hook() {

    // check for unsupported platforms
#if defined(__aarch64__)
    return hookAArch64();
#elif !defined(__x86_64__)
    return false;
#endif

    if (g_pFunctionHookSystem->m_activeHooks.contains(rc<uint64_t>(m_source))) {
        // TODO: return actual error codes...
        LOG(Log::ERR, "[functionhook] failed, function is already hooked");
        return false;
    }

    // jmp rel32
    // offset for relative addr: 1
    static constexpr uint8_t RELATIVE_JMP_ADDRESS[]      = {0xE9, 0x00, 0x00, 0x00, 0x00};
    static constexpr size_t  RELATIVE_JMP_ADDRESS_OFFSET = 1;
    // movabs $0,%rax | jmpq *rax
    static constexpr uint8_t ABSOLUTE_JMP_ADDRESS[]      = {0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0};
    static constexpr size_t  ABSOLUTE_JMP_ADDRESS_OFFSET = 2;
    // nop
    static constexpr uint8_t NOP = 0x90;

    // alloc trampolines
    const auto MAX_TRAMPOLINE_SIZE = HOOK_TRAMPOLINE_MAX_SIZE; // we will never need more.
    m_launchTrampolineAddr         = rc<void*>(g_pFunctionHookSystem->getAddressForTrampo());
    m_landTrampolineAddr           = rc<void*>(g_pFunctionHookSystem->getAddressForTrampo());

    // probe instructions to be trampolin'd
    SInstructionProbe probe;
    try {
        probe = probeMinimumJumpSize(m_source, sizeof(RELATIVE_JMP_ADDRESS));
    } catch (std::exception& e) { return false; }

    const auto PROBEFIXEDASM = fixInstructionProbeRIPCalls(probe);

    if (PROBEFIXEDASM.bytes.empty()) {
        LOG(Log::ERR, "[functionhook] failed, unsupported asm / failed assembling:\n{}", probe.assembly);
        return false;
    }

    if (std::abs(rc<int64_t>(m_source) - rc<int64_t>(m_landTrampolineAddr)) > 2000000000 /* 2 GB */) {
        LOG(Log::ERR, "[functionhook] failed, source and trampo are over 2GB apart");
        return false;
    }

    const size_t HOOKSIZE = PROBEFIXEDASM.bytes.size();
    const size_t ORIGSIZE = probe.len;

    const auto   TRAMPOLINE_SIZE = sizeof(RELATIVE_JMP_ADDRESS) + HOOKSIZE;

    if (TRAMPOLINE_SIZE > MAX_TRAMPOLINE_SIZE) {
        LOG(Log::ERR, "[functionhook] failed, not enough space in trampo to alloc:\n{}", probe.assembly);
        return false;
    }

    m_originalBytes.resize(ORIGSIZE);
    memcpy(m_originalBytes.data(), m_source, ORIGSIZE);

    // populate land trampoline
    memcpy(m_landTrampolineAddr, PROBEFIXEDASM.bytes.data(), HOOKSIZE);                                        // first, original but fixed func bytes
    memcpy(sc<uint8_t*>(m_landTrampolineAddr) + HOOKSIZE, RELATIVE_JMP_ADDRESS, sizeof(RELATIVE_JMP_ADDRESS)); // then, jump to source

    // populate short jump addr
    *rc<int32_t*>(sc<uint8_t*>(m_landTrampolineAddr) + TRAMPOLINE_SIZE - sizeof(RELATIVE_JMP_ADDRESS) + RELATIVE_JMP_ADDRESS_OFFSET) =
        sc<int64_t>((sc<uint8_t*>(m_source) + probe.len)                     // jump to source + probe len (skip header)
                    - (sc<uint8_t*>(m_landTrampolineAddr) + TRAMPOLINE_SIZE) // from trampo + size - jmp (not - size because jmp is rel to rip after instr)
        );

    // populate launch trampoline
    memcpy(m_launchTrampolineAddr, ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS)); // long jump to our hk

    // populate long jump addr
    *rc<uint64_t*>(sc<uint8_t*>(m_launchTrampolineAddr) + ABSOLUTE_JMP_ADDRESS_OFFSET) = rc<uint64_t>(m_destination); // long jump to hk fn

    // make short jump to launch trampoile
    const auto     PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
    const uint8_t* PROTSTART    = sc<uint8_t*>(m_source) - (rc<uint64_t>(m_source) % PAGESIZE_VAR);
    const size_t   PROTLEN      = std::ceil(sc<float>(ORIGSIZE + (rc<uint64_t>(m_source) - rc<uint64_t>(PROTSTART))) / sc<float>(PAGESIZE_VAR)) * PAGESIZE_VAR;
    mprotect(const_cast<uint8_t*>(PROTSTART), PROTLEN, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(m_source, RELATIVE_JMP_ADDRESS, sizeof(RELATIVE_JMP_ADDRESS));

    size_t currentOp = sizeof(RELATIVE_JMP_ADDRESS);
    memset(sc<uint8_t*>(m_source) + currentOp, NOP, ORIGSIZE - currentOp);

    // populate short jump addr
    *rc<int32_t*>(sc<uint8_t*>(m_source) + RELATIVE_JMP_ADDRESS_OFFSET) = sc<int32_t>( //
        rc<uint64_t>(m_launchTrampolineAddr)                                           // jump to the launch trampoline which jumps to hk
        - (rc<uint64_t>(m_source) + 5)                                                 // from source
    );

    // revert mprot
    mprotect(const_cast<uint8_t*>(PROTSTART), PROTLEN, PROT_READ | PROT_EXEC);

    // set original addr to land trampo addr
    m_original = m_landTrampolineAddr;

    m_active  = true;
    m_hookLen = ORIGSIZE;

    g_pFunctionHookSystem->m_activeHooks.emplace(rc<uint64_t>(m_source));

    return true;
}

bool CFunctionHook::unhook() {
    // check for unsupported platforms
#if !defined(__x86_64__) && !defined(__aarch64__)
    return false;
#endif

    if (!m_active)
        return false;

    g_pFunctionHookSystem->m_activeHooks.erase(rc<uint64_t>(m_source));

#if defined(__aarch64__)
    writeCodeAArch64(m_source, m_originalBytes.data(), m_hookLen);
#else
    // allow write to src
    mprotect(sc<uint8_t*>(m_source) - rc<uint64_t>(m_source) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);

    // write back original bytes
    memcpy(m_source, m_originalBytes.data(), m_hookLen);

    // revert mprot
    mprotect(sc<uint8_t*>(m_source) - rc<uint64_t>(m_source) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);
#endif

    // reset vars
    m_active               = false;
    m_hookLen              = 0;
    m_landTrampolineAddr   = nullptr; // no unmapping, it's managed by the HookSystem
    m_launchTrampolineAddr = nullptr; // no unmapping, it's managed by the HookSystem
    m_original             = nullptr;
    m_originalBytes.clear();

    return true;
}

CFunctionHook* CHookSystem::initHook(HANDLE owner, void* source, void* destination) {
    return m_hooks.emplace_back(makeUnique<CFunctionHook>(owner, source, destination)).get();
}

bool CHookSystem::removeHook(CFunctionHook* hook) {
    std::erase_if(m_hooks, [&](const auto& other) { return other.get() == hook; });
    return true; // todo: make false if not found
}

void CHookSystem::removeAllHooksFrom(HANDLE handle) {
    std::erase_if(m_hooks, [&](const auto& other) { return other->m_owner == handle; });
}

static uintptr_t seekNewPageAddr() {
    const uint64_t PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
    auto           MAPS         = std::ifstream("/proc/self/maps");

    uint64_t       lastStart = 0, lastEnd = 0;

    bool           anchoredToHyprland = false;

    std::string    line;
    while (std::getline(MAPS, line)) {
        CVarList props{line, 0, 's', true};

        uint64_t start = 0, end = 0;
        if (props[0].empty()) {
            LOG(Log::WARN, "seekNewPageAddr: unexpected line in self maps");
            continue;
        }

        CVarList startEnd{props[0], 0, '-', true};

        try {
            start = std::stoull(startEnd[0], nullptr, 16);
            end   = std::stoull(startEnd[1], nullptr, 16);
        } catch (std::exception& e) {
            LOG(Log::WARN, "seekNewPageAddr: unexpected line in self maps: {}", line);
            continue;
        }

        LOG(Log::DEBUG, "seekNewPageAddr: page 0x{:x} - 0x{:x}", start, end);

        if (lastStart == 0) {
            lastStart = start;
            lastEnd   = end;
            continue;
        }

        if (!anchoredToHyprland && line.contains("Hyprland")) {
            LOG(Log::DEBUG, "seekNewPageAddr: Anchored to hyprland at 0x{:x}", start);
            anchoredToHyprland = true;
        } else if (start - lastEnd > PAGESIZE_VAR * 2) {
            if (!anchoredToHyprland) {
                LOG(Log::DEBUG, "seekNewPageAddr: skipping gap 0x{:x}-0x{:x}, not anchored to Hyprland code pages yet.", lastEnd, start);
                lastStart = start;
                lastEnd   = end;
                continue;
            }

            LOG(Log::DEBUG, "seekNewPageAddr: found gap: 0x{:x}-0x{:x} ({} bytes)", lastEnd, start, start - lastEnd);
            MAPS.close();
            return lastEnd;
        }

        lastStart = start;
        lastEnd   = end;
    }

    MAPS.close();
    return 0;
}

uint64_t CHookSystem::getAddressForTrampo() {
    // yes, technically this creates a memory leak of 64B every hook creation. But I don't care.
    // tracking all the users of the memory would be painful.
    // Nobody will hook 100k times, and even if, that's only 6.4 MB. Nothing.

    SAllocatedPage* page = nullptr;
    for (auto& p : m_pages) {
        if (p.used + HOOK_TRAMPOLINE_MAX_SIZE > p.len)
            continue;

        page = &p;
        break;
    }

    if (!page)
        page = &m_pages.emplace_back();

    if (!page->addr) {
        // allocate it
        LOG(Log::DEBUG, "getAddressForTrampo: Allocating new page for hooks");
        const uint64_t PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
        const auto     BASEPAGEADDR = seekNewPageAddr();
        for (int attempt = 0; attempt < 2; ++attempt) {
            for (int i = 0; i <= 2; ++i) {
                const auto PAGEADDR = BASEPAGEADDR + i * PAGESIZE_VAR;

                page->addr = rc<uint64_t>(mmap(rc<void*>(PAGEADDR), PAGESIZE_VAR, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
                page->len  = PAGESIZE_VAR;
                page->used = 0;

                LOG(Log::DEBUG, "Attempted to allocate 0x{:x}, got 0x{:x}", PAGEADDR, page->addr);

                if (page->addr == rc<uint64_t>(MAP_FAILED))
                    continue;
                if (page->addr != PAGEADDR && attempt == 0) {
                    munmap(rc<void*>(page->addr), PAGESIZE_VAR);
                    page->addr = 0;
                    page->len  = 0;
                    continue;
                }

                break;
            }
            if (page->addr)
                break;
        }
    }

    const auto ADDRFORCONSUMER = page->addr + page->used;

    page->used += HOOK_TRAMPOLINE_MAX_SIZE;

    LOG(Log::DEBUG, "getAddressForTrampo: Returning addr 0x{:x} for page at 0x{:x}", ADDRFORCONSUMER, page->addr);

    return ADDRFORCONSUMER;
}
