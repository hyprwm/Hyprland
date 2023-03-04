#include "HookSystem.hpp"

#define register
#include <udis86.h>
#undef register
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

CFunctionHook::CFunctionHook(HANDLE owner, void* source, void* destination) {
    m_pSource      = source;
    m_pDestination = destination;
    m_pOwner       = owner;
}

CFunctionHook::~CFunctionHook() {
    if (m_bActive) {
        unhook();
    }
}

size_t CFunctionHook::getInstructionLenAt(void* start) {
    ud_t udis;

    ud_init(&udis);
    ud_set_mode(&udis, 64);
    ud_set_syntax(&udis, UD_SYN_INTEL);

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

    if (!ins.empty() && ins.find("rip") != std::string::npos) {
        // todo: support something besides call qword ptr [rip + 0xdeadbeef]
        // I don't have an assembler. I don't think udis provides one. Besides, variables might be tricky.
        if (((uint8_t*)start)[0] == 0xFF && ((uint8_t*)start)[1] == 0x15)
            m_vTrampolineRIPUses.emplace_back(std::make_pair<>((uint64_t)start - (uint64_t)m_pSource, ins));
    }

    return insSize;
}

size_t CFunctionHook::probeMinimumJumpSize(void* start, size_t min) {

    size_t size = 0;

    while (size <= min) {
        // find info about this instruction
        size_t insLen = getInstructionLenAt((uint8_t*)start + size);
        size += insLen;
    }

    return size;
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
    /*
        movabs $0,%rax
        callq *%rax

        offset for addr: 3
    */
    static constexpr uint8_t CALL_WITH_RAX[]              = {0x48, 0xB8, 0xEF, 0xBE, 0xAD, 0xDE, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x10};
    static constexpr size_t  CALL_WITH_RAX_ADDRESS_OFFSET = 2;

    // get minimum size to overwrite
    const auto HOOKSIZE = probeMinimumJumpSize(m_pSource, sizeof(ABSOLUTE_JMP_ADDRESS) + sizeof(PUSH_RAX) + sizeof(POP_RAX));

    // alloc trampoline
    const auto TRAMPOLINE_SIZE = sizeof(ABSOLUTE_JMP_ADDRESS) + HOOKSIZE + sizeof(PUSH_RAX) + m_vTrampolineRIPUses.size() * (sizeof(CALL_WITH_RAX) - 6);
    m_pTrampolineAddr          = mmap(NULL, TRAMPOLINE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    m_pOriginalBytes = malloc(HOOKSIZE);
    memcpy(m_pOriginalBytes, m_pSource, HOOKSIZE);

    // populate trampoline
    memcpy(m_pTrampolineAddr, m_pSource, HOOKSIZE);                                                              // first, original func bytes
    memcpy((uint8_t*)m_pTrampolineAddr + HOOKSIZE, PUSH_RAX, sizeof(PUSH_RAX));                                            // then, pushq %rax
    memcpy((uint8_t*)m_pTrampolineAddr + HOOKSIZE + sizeof(PUSH_RAX), ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS)); // then, jump to source

    // fix trampoline %rip calls
    for (size_t i = 0; i < m_vTrampolineRIPUses.size(); ++i) {
        size_t callOffset      = i * (sizeof(CALL_WITH_RAX) - 6 /* callq [rip + x] */) + m_vTrampolineRIPUses[i].first;
        size_t realCallAddress = (uint64_t)m_pSource + callOffset + 6 + *((uint32_t*)((uint8_t*)m_pSource + callOffset + 2));

        memmove((uint8_t*)m_pTrampolineAddr + callOffset + sizeof(CALL_WITH_RAX), (uint8_t*)m_pTrampolineAddr + callOffset + 6, TRAMPOLINE_SIZE - callOffset - 6);
        memcpy((uint8_t*)m_pTrampolineAddr + callOffset, CALL_WITH_RAX, sizeof(CALL_WITH_RAX));

        *(uint64_t*)((uint8_t*)m_pTrampolineAddr + callOffset + CALL_WITH_RAX_ADDRESS_OFFSET) = (uint64_t)realCallAddress;
    }

    // fixup trampoline addr
    *(uint64_t*)((uint8_t*)m_pTrampolineAddr + TRAMPOLINE_SIZE - sizeof(ABSOLUTE_JMP_ADDRESS) + ABSOLUTE_JMP_ADDRESS_OFFSET) = (uint64_t)((uint8_t*)m_pSource + sizeof(ABSOLUTE_JMP_ADDRESS));

    // make jump to hk
    mprotect((uint8_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(m_pSource, ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS));

    // make popq %rax and NOP all remaining
    memcpy((uint8_t*)m_pSource + sizeof(ABSOLUTE_JMP_ADDRESS), POP_RAX, sizeof(POP_RAX));
    size_t currentOp = sizeof(ABSOLUTE_JMP_ADDRESS) + sizeof(POP_RAX);
    memset((uint8_t*)m_pSource + currentOp, NOP, HOOKSIZE - currentOp);

    // fixup jump addr
    *(uint64_t*)((uint8_t*)m_pSource + ABSOLUTE_JMP_ADDRESS_OFFSET) = (uint64_t)(m_pDestination);

    // revert mprot
    mprotect((uint8_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);

    // set original addr to trampo addr
    m_pOriginal = m_pTrampolineAddr;

    m_bActive    = true;
    m_iHookLen   = HOOKSIZE;
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

    // unmap
    munmap(m_pTrampolineAddr, m_iTrampoLen);

    // reset vars
    m_bActive         = false;
    m_iHookLen        = 0;
    m_iTrampoLen      = 0;
    m_pTrampolineAddr = nullptr;
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
