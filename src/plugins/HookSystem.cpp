#include "HookSystem.hpp"
#include "../debug/Log.hpp"
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
        instrs += probe.assembly + "\n";
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
            auto        addrResult   = configStringToInt(addr);
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

bool CFunctionHook::hook() {

    // check for unsupported platforms
#if !defined(__x86_64__)
    return false;
#endif

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
        Debug::log(ERR, "[functionhook] failed, unsupported asm / failed assembling:\n{}", probe.assembly);
        return false;
    }

    if (std::abs(rc<int64_t>(m_source) - rc<int64_t>(m_landTrampolineAddr)) > 2000000000 /* 2 GB */) {
        Debug::log(ERR, "[functionhook] failed, source and trampo are over 2GB apart");
        return false;
    }

    const size_t HOOKSIZE = PROBEFIXEDASM.bytes.size();
    const size_t ORIGSIZE = probe.len;

    const auto   TRAMPOLINE_SIZE = sizeof(RELATIVE_JMP_ADDRESS) + HOOKSIZE;

    if (TRAMPOLINE_SIZE > MAX_TRAMPOLINE_SIZE) {
        Debug::log(ERR, "[functionhook] failed, not enough space in trampo to alloc:\n{}", probe.assembly);
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

    return true;
}

bool CFunctionHook::unhook() {
    // check for unsupported platforms
#if !defined(__x86_64__)
    return false;
#endif

    if (!m_active)
        return false;

    // allow write to src
    mprotect(sc<uint8_t*>(m_source) - rc<uint64_t>(m_source) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);

    // write back original bytes
    memcpy(m_source, m_originalBytes.data(), m_hookLen);

    // revert mprot
    mprotect(sc<uint8_t*>(m_source) - rc<uint64_t>(m_source) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);

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
            Debug::log(WARN, "seekNewPageAddr: unexpected line in self maps");
            continue;
        }

        CVarList startEnd{props[0], 0, '-', true};

        try {
            start = std::stoull(startEnd[0], nullptr, 16);
            end   = std::stoull(startEnd[1], nullptr, 16);
        } catch (std::exception& e) {
            Debug::log(WARN, "seekNewPageAddr: unexpected line in self maps: {}", line);
            continue;
        }

        Debug::log(LOG, "seekNewPageAddr: page 0x{:x} - 0x{:x}", start, end);

        if (lastStart == 0) {
            lastStart = start;
            lastEnd   = end;
            continue;
        }

        if (!anchoredToHyprland && line.contains("Hyprland")) {
            Debug::log(LOG, "seekNewPageAddr: Anchored to hyprland at 0x{:x}", start);
            anchoredToHyprland = true;
        } else if (start - lastEnd > PAGESIZE_VAR * 2) {
            if (!anchoredToHyprland) {
                Debug::log(LOG, "seekNewPageAddr: skipping gap 0x{:x}-0x{:x}, not anchored to Hyprland code pages yet.", lastEnd, start);
                lastStart = start;
                lastEnd   = end;
                continue;
            }

            Debug::log(LOG, "seekNewPageAddr: found gap: 0x{:x}-0x{:x} ({} bytes)", lastEnd, start, start - lastEnd);
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
        Debug::log(LOG, "getAddressForTrampo: Allocating new page for hooks");
        const uint64_t PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
        const auto     BASEPAGEADDR = seekNewPageAddr();
        for (int attempt = 0; attempt < 2; ++attempt) {
            for (int i = 0; i <= 2; ++i) {
                const auto PAGEADDR = BASEPAGEADDR + i * PAGESIZE_VAR;

                page->addr = rc<uint64_t>(mmap(rc<void*>(PAGEADDR), PAGESIZE_VAR, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
                page->len  = PAGESIZE_VAR;
                page->used = 0;

                Debug::log(LOG, "Attempted to allocate 0x{:x}, got 0x{:x}", PAGEADDR, page->addr);

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

    Debug::log(LOG, "getAddressForTrampo: Returning addr 0x{:x} for page at 0x{:x}", ADDRFORCONSUMER, page->addr);

    return ADDRFORCONSUMER;
}
