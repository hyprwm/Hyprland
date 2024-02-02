#include "HookSystem.hpp"
#include "../debug/Log.hpp"
#include "../helpers/VarList.hpp"

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
    // analyze the code and fix what we know how to.
    uint64_t currentAddress = (uint64_t)m_pSource;
    // actually newline + 1
    size_t      lastAsmNewline = 0;
    std::string assemblyBuilder;
    for (auto& len : probe.insSizes) {

        std::string code = probe.assembly.substr(lastAsmNewline, probe.assembly.find("\n", lastAsmNewline) - lastAsmNewline);
        if (code.contains("%rip")) {
            CVarList       tokens{code, 0, 's'};
            size_t         plusPresent  = tokens[1][0] == '+' ? 1 : 0;
            size_t         minusPresent = tokens[1][0] == '-' ? 1 : 0;
            std::string    addr         = tokens[1].substr((plusPresent || minusPresent), tokens[1].find("(%rip)") - (plusPresent || minusPresent));
            const uint64_t OFFSET       = (minusPresent ? -1 : 1) * configStringToInt(addr);
            if (OFFSET == 0)
                return {};
            const uint64_t DESTINATION = currentAddress + OFFSET + len;

            if (code.starts_with("mov")) {
                // mov +0xdeadbeef(%rip), %rax
                assemblyBuilder += std::format("movabs $0x{:x}, {}\n", DESTINATION, tokens[2]);
            } else if (code.starts_with("call")) {
                // call +0xdeadbeef(%rip)
                assemblyBuilder += std::format("pushq %rax\nmovabs $0x{:x}, %rax\ncallq *%rax\npopq %rax\n", DESTINATION);
            } else if (code.starts_with("lea")) {
                // lea 0xdeadbeef(%rip), %rax
                assemblyBuilder += std::format("movabs $0x{:x}, {}\n", DESTINATION, tokens[2]);
            } else {
                return {};
            }
        } else if (code.contains("invalid")) {
            std::vector<uint8_t> bytes;
            bytes.resize(len);
            memcpy(bytes.data(), (std::byte*)currentAddress, len);
            if (len == 4 && bytes[0] == 0xF3 && bytes[1] == 0x0F && bytes[2] == 0x1E && bytes[3] == 0xFA) {
                // F3 0F 1E FA = endbr64, udis doesn't understand that one
                assemblyBuilder += "endbr64\n";
            } else {
                // raise error, unknown op
                std::string strBytes;
                for (auto& b : bytes) {
                    strBytes += std::format("{:x} ", b);
                }
                Debug::log(ERR, "[functionhook] unknown bytes: {}", strBytes);
                return {};
            }
        } else {
            assemblyBuilder += code + "\n";
        }

        lastAsmNewline = probe.assembly.find("\n", lastAsmNewline) + 1;
        currentAddress += len;
    }

    std::ofstream ofs("/tmp/hypr/.hookcode.asm", std::ios::trunc);
    ofs << assemblyBuilder;
    ofs.close();
    std::string ret = execAndGet(
        "cc -x assembler -c /tmp/hypr/.hookcode.asm -o /tmp/hypr/.hookbinary.o 2>&1 && objcopy -O binary -j .text /tmp/hypr/.hookbinary.o /tmp/hypr/.hookbinary2.o 2>&1");
    Debug::log(LOG, "[functionhook] assembler returned:\n{}", ret);
    if (!std::filesystem::exists("/tmp/hypr/.hookbinary2.o")) {
        std::filesystem::remove("/tmp/hypr/.hookcode.asm");
        std::filesystem::remove("/tmp/hypr/.hookbinary.asm");
        return {};
    }

    std::ifstream ifs("/tmp/hypr/.hookbinary2.o", std::ios::binary);
    SAssembly     returns = {std::vector<char>(std::istreambuf_iterator<char>(ifs), {})};
    ifs.close();
    std::filesystem::remove("/tmp/hypr/.hookcode.asm");
    std::filesystem::remove("/tmp/hypr/.hookbinary.o");
    std::filesystem::remove("/tmp/hypr/.hookbinary2.o");

    return returns;
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

    // alloc trampoline
    const auto TRAMPOLINE_SIZE = sizeof(ABSOLUTE_JMP_ADDRESS) + HOOKSIZE + sizeof(PUSH_RAX);
    m_pTrampolineAddr          = mmap(NULL, TRAMPOLINE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

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
