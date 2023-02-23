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

size_t probeMinimumJumpSize(void* start, size_t min) {
    int  currentOffset = 1;

    ud_t udis;

    ud_init(&udis);
    ud_set_mode(&udis, 64);

    while (true) {
        ud_set_input_buffer(&udis, (uint8_t*)start, currentOffset);
        int size = ud_disassemble(&udis);
        if (size != currentOffset && currentOffset > min) {
            break;
        }
        currentOffset++;
    }

    return currentOffset;
}

bool CFunctionHook::hook() {

    // check for unsupported platforms
#if !defined(__x86_64__)
    return false;
#endif

    // movabs $0,%rax | jmpq *%rax
    static constexpr uint8_t ABSOLUTE_JMP_ADDRESS[] = {0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0};

    // get minimum size to overwrite
    const auto HOOKSIZE = probeMinimumJumpSize(m_pSource, sizeof(ABSOLUTE_JMP_ADDRESS));

    // alloc trampoline
    m_pTrampolineAddr = mmap(NULL, sizeof(ABSOLUTE_JMP_ADDRESS) + HOOKSIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    // populate trampoline
    memcpy(m_pTrampolineAddr, m_pSource, HOOKSIZE);                                                      // first, original func bytes
    memcpy((uint64_t*)m_pTrampolineAddr + HOOKSIZE, ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS)); // then, our jump back

    // fixup trampoline addr
    *(uint64_t*)((uint64_t*)m_pTrampolineAddr + HOOKSIZE + 2) = (uint64_t)((uint64_t*)m_pSource + HOOKSIZE);

    // make jump to hk
    mprotect((uint64_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy((uint64_t*)m_pSource, ABSOLUTE_JMP_ADDRESS, sizeof(ABSOLUTE_JMP_ADDRESS));

    // fixup jump addr
    *(uint64_t*)(m_pSource + 2) = (uint64_t)(m_pDestination);

    // revert mprot
    mprotect((uint64_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);

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
    mprotect((uint64_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);

    // write back original bytes
    memcpy(m_pSource, m_pTrampolineAddr, m_iHookLen);

    // revert mprot
    mprotect((uint64_t*)m_pSource - ((uint64_t)m_pSource) % sysconf(_SC_PAGE_SIZE), sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);

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