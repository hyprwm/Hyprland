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

size_t getInstructionLenAt(void* start) {
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

    return insSize;
}

size_t probeMinimumJumpSize(void* start, size_t min) {

    size_t size = 0;

    while (size <= min) {
        // find info about this instruction
        size_t insLen = getInstructionLenAt(start + size);
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
    static constexpr uint8_t ABSOLUTE_JMP_ADDRESS[] = {0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0};
    // pushq %rax
    static constexpr uint8_t PUSH_RAX[] = {0x50};
    // popq %rax
    static constexpr uint8_t POP_RAX[] = {0x58};
    // nop
    static constexpr uint8_t NOP = 0x90;

    // get minimum size to overwrite
    const auto HOOKSIZE = probeMinimumJumpSize(m_pSource, sizeof(ABSOLUTE_JMP_ADDRESS) + sizeof(PUSH_RAX) + sizeof(POP_RAX));

    // alloc trampoline
    m_pTrampolineAddr = mmap(NULL, sizeof(ABSOLUTE_JMP_ADDRESS) + HOOKSIZE + sizeof(PUSH_RAX), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    // populate trampoline
    memcpy(m_pTrampolineAddr, m_pSource, HOOKSIZE);                                                              // first, original func bytes
    memcpy(m_pTrampolineAddr + HOOKSIZE, PUSH_RAX, sizeof(PUSH_RAX));                                            // then, pushq %rax
    memcpy(m_pTrampolineAddr + HOOKSIZE + sizeof(PUSH_RAX), ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS)); // then, jump to source

    // fixup trampoline addr
    *(uint64_t*)(m_pTrampolineAddr + HOOKSIZE + 2 + sizeof(PUSH_RAX)) = (uint64_t)(m_pSource + sizeof(ABSOLUTE_JMP_ADDRESS));

    // make jump to hk
    mprotect(m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(m_pSource, ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS));

    // make popq %rax and NOP all remaining
    memcpy(m_pSource + sizeof(ABSOLUTE_JMP_ADDRESS), POP_RAX, sizeof(POP_RAX));
    size_t currentOp = sizeof(ABSOLUTE_JMP_ADDRESS) + sizeof(POP_RAX);
    memset(m_pSource + currentOp, NOP, HOOKSIZE - currentOp);

    // fixup jump addr
    *(uint64_t*)(m_pSource + 2) = (uint64_t)(m_pDestination);

    // revert mprot
    mprotect(m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);

    // set original addr to trampo addr
    m_pOriginal = m_pTrampolineAddr;

    m_bActive    = true;
    m_iHookLen   = HOOKSIZE;
    m_iTrampoLen = HOOKSIZE + sizeof(ABSOLUTE_JMP_ADDRESS);

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
    mprotect(m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);

    // write back original bytes
    memcpy(m_pSource, m_pTrampolineAddr, m_iHookLen);

    // revert mprot
    mprotect(m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);

    // unmap
    munmap(m_pTrampolineAddr, m_iTrampoLen);

    // reset vars
    m_bActive         = false;
    m_iHookLen        = 0;
    m_iTrampoLen      = 0;
    m_pTrampolineAddr = nullptr;

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