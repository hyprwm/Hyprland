#include "HookSystem.hpp"
#include "../debug/Log.hpp"
#include "../helpers/varlist/VarList.hpp"
#include "../managers/TokenManager.hpp"
#include "../Compositor.hpp"

#define register
#include <udis86.h>
#undef register
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

CFunctionHook::CFunctionHook(HANDLE owner, void* source, void* destination) : m_pSource(source), m_pDestination(destination), m_pOwner(owner) {
    ;
}

CFunctionHook::~CFunctionHook() {
    if (m_bActive)
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
        ud_set_input_buffer(&udis, (uint8_t*)start, curOffset);
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
        auto probe = getInstructionLenAt((uint8_t*)start + size);
        sizes.push_back(probe.len);
        size += probe.len;
        instrs += probe.assembly + "\n";
    }

    return {size, instrs, sizes};
}

CFunctionHook::SAssembly CFunctionHook::fixInstructionProbeRIPCalls(const SInstructionProbe& probe) {
    SAssembly returns;

    // analyze the code and fix what we know how to.
    uint64_t currentAddress = (uint64_t)m_pSource;
    // actually newline + 1
    size_t lastAsmNewline = 0;
    // needle for destination binary
    size_t            currentDestinationOffset = 0;

    std::vector<char> finalBytes;
    finalBytes.resize(probe.len);

    for (auto const& len : probe.insSizes) {

        // copy original bytes to our finalBytes
        for (size_t i = 0; i < len; ++i) {
            finalBytes[currentDestinationOffset + i] = *(char*)(currentAddress + i);
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

            const uint64_t PREDICTEDRIP = (uint64_t)m_pTrampolineAddr + currentDestinationOffset + len;
            const int32_t  NEWRIPOFFSET = DESTINATION - PREDICTEDRIP;

            size_t         ripOffset = 0;

            // find %rip usage offset from beginning
            for (int i = len - 4 /* 32-bit */; i > 0; --i) {
                if (*(int32_t*)(currentAddress + i) == OFFSET) {
                    ripOffset = i;
                    break;
                }
            }

            if (ripOffset == 0)
                return {};

            // fix offset in the final bytes. This doesn't care about endianness
            *(int32_t*)&finalBytes[currentDestinationOffset + ripOffset] = NEWRIPOFFSET;

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

    // movabs $0,%rax | jmpq *%rax
    // offset for addr: 2
    static constexpr uint8_t ABSOLUTE_JMP_ADDRESS[]      = {0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0};
    static constexpr size_t  ABSOLUTE_JMP_ADDRESS_OFFSET = 2;
    // pushq %rax
    static constexpr uint8_t PUSH_RAX[] = {0x50};
    // popq %rax
    static constexpr uint8_t POP_RAX[] = {0x58};
    // nop
    static constexpr uint8_t NOP = 0x90;

    // alloc trampoline
    const auto MAX_TRAMPOLINE_SIZE = HOOK_TRAMPOLINE_MAX_SIZE; // we will never need more.
    m_pTrampolineAddr              = (void*)g_pFunctionHookSystem->getAddressForTrampo();

    // probe instructions to be trampolin'd
    SInstructionProbe probe;
    try {
        probe = probeMinimumJumpSize(m_pSource, sizeof(ABSOLUTE_JMP_ADDRESS) + sizeof(PUSH_RAX) + sizeof(POP_RAX));
    } catch (std::exception& e) { return false; }

    const auto PROBEFIXEDASM = fixInstructionProbeRIPCalls(probe);

    if (PROBEFIXEDASM.bytes.size() == 0) {
        Debug::log(ERR, "[functionhook] failed, unsupported asm / failed assembling:\n{}", probe.assembly);
        return false;
    }

    const size_t HOOKSIZE = PROBEFIXEDASM.bytes.size();
    const size_t ORIGSIZE = probe.len;

    const auto   TRAMPOLINE_SIZE = sizeof(ABSOLUTE_JMP_ADDRESS) + HOOKSIZE + sizeof(PUSH_RAX);

    if (TRAMPOLINE_SIZE > MAX_TRAMPOLINE_SIZE) {
        Debug::log(ERR, "[functionhook] failed, not enough space in trampo to alloc:\n{}", probe.assembly);
        return false;
    }

    m_pOriginalBytes = malloc(ORIGSIZE);
    memcpy(m_pOriginalBytes, m_pSource, ORIGSIZE);

    // populate trampoline
    memcpy(m_pTrampolineAddr, PROBEFIXEDASM.bytes.data(), HOOKSIZE);                                                       // first, original but fixed func bytes
    memcpy((uint8_t*)m_pTrampolineAddr + HOOKSIZE, PUSH_RAX, sizeof(PUSH_RAX));                                            // then, pushq %rax
    memcpy((uint8_t*)m_pTrampolineAddr + HOOKSIZE + sizeof(PUSH_RAX), ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS)); // then, jump to source

    // fixup trampoline addr
    *(uint64_t*)((uint8_t*)m_pTrampolineAddr + TRAMPOLINE_SIZE - sizeof(ABSOLUTE_JMP_ADDRESS) + ABSOLUTE_JMP_ADDRESS_OFFSET) =
        (uint64_t)((uint8_t*)m_pSource + sizeof(ABSOLUTE_JMP_ADDRESS));

    // make jump to hk
    const auto     PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
    const uint8_t* PROTSTART    = (uint8_t*)m_pSource - ((uint64_t)m_pSource % PAGESIZE_VAR);
    const size_t   PROTLEN      = std::ceil((float)(ORIGSIZE + ((uint64_t)m_pSource - (uint64_t)PROTSTART)) / (float)PAGESIZE_VAR) * PAGESIZE_VAR;
    mprotect((uint8_t*)PROTSTART, PROTLEN, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy((uint8_t*)m_pSource, ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS));

    // make popq %rax and NOP all remaining
    memcpy((uint8_t*)m_pSource + sizeof(ABSOLUTE_JMP_ADDRESS), POP_RAX, sizeof(POP_RAX));
    size_t currentOp = sizeof(ABSOLUTE_JMP_ADDRESS) + sizeof(POP_RAX);
    memset((uint8_t*)m_pSource + currentOp, NOP, ORIGSIZE - currentOp);

    // fixup jump addr
    *(uint64_t*)((uint8_t*)m_pSource + ABSOLUTE_JMP_ADDRESS_OFFSET) = (uint64_t)(m_pDestination);

    // revert mprot
    mprotect((uint8_t*)PROTSTART, PROTLEN, PROT_READ | PROT_EXEC);

    // set original addr to trampo addr
    m_pOriginal = m_pTrampolineAddr;

    m_bActive    = true;
    m_iHookLen   = ORIGSIZE;
    m_iTrampoLen = TRAMPOLINE_SIZE;

    return true;
}

bool CFunctionHook::unhook() {
    // check for unsupported platforms
#if !defined(__x86_64__)
    return false;
#endif

    if (!m_bActive)
        return false;

    // allow write to src
    mprotect((uint8_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);

    // write back original bytes
    memcpy(m_pSource, m_pOriginalBytes, m_iHookLen);

    // revert mprot
    mprotect((uint8_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);

    // reset vars
    m_bActive         = false;
    m_iHookLen        = 0;
    m_iTrampoLen      = 0;
    m_pTrampolineAddr = nullptr; // no unmapping, it's managed by the HookSystem
    m_pOriginalBytes  = nullptr;

    free(m_pOriginalBytes);

    return true;
}

CFunctionHook* CHookSystem::initHook(HANDLE owner, void* source, void* destination) {
    return m_vHooks.emplace_back(std::make_unique<CFunctionHook>(owner, source, destination)).get();
}

bool CHookSystem::removeHook(CFunctionHook* hook) {
    std::erase_if(m_vHooks, [&](const auto& other) { return other.get() == hook; });
    return true; // todo: make false if not found
}

void CHookSystem::removeAllHooksFrom(HANDLE handle) {
    std::erase_if(m_vHooks, [&](const auto& other) { return other->m_pOwner == handle; });
}

static uintptr_t seekNewPageAddr() {
    const uint64_t PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
    auto           MAPS         = std::ifstream("/proc/self/maps");

    uint64_t       lastStart = 0, lastEnd = 0;

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

        if (start - lastEnd > PAGESIZE_VAR * 2) {
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
    for (auto& p : pages) {
        if (p.used + HOOK_TRAMPOLINE_MAX_SIZE > p.len)
            continue;

        page = &p;
        break;
    }

    if (!page)
        page = &pages.emplace_back();

    if (!page->addr) {
        // allocate it
        Debug::log(LOG, "getAddressForTrampo: Allocating new page for hooks");
        const uint64_t PAGESIZE_VAR = sysconf(_SC_PAGE_SIZE);
        const auto     BASEPAGEADDR = seekNewPageAddr();
        for (int attempt = 0; attempt < 2; ++attempt) {
            for (int i = 0; i <= 2; ++i) {
                const auto PAGEADDR = BASEPAGEADDR + i * PAGESIZE_VAR;

                page->addr = (uint64_t)mmap((void*)PAGEADDR, PAGESIZE_VAR, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                page->len  = PAGESIZE_VAR;
                page->used = 0;

                Debug::log(LOG, "Attempted to allocate 0x{:x}, got 0x{:x}", PAGEADDR, page->addr);

                if (page->addr == (uint64_t)MAP_FAILED)
                    continue;
                if (page->addr != PAGEADDR && attempt == 0) {
                    munmap((void*)page->addr, PAGESIZE_VAR);
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