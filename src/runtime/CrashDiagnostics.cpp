#include "CrashDiagnostics.hpp"
#include <cstdint>
#include <csignal>
#include <initializer_list>
#include <unistd.h>
#include <sys/ucontext.h>
#if defined(__APPLE__) && defined(__aarch64__)
#include <mach/arm/thread_status.h>
#endif

namespace {
volatile sig_atomic_t reporting = 0;

// Only fixed local storage and async-signal-safe writes in the signal handler.
// Unwinding and dladdr can deadlock when the interrupted thread held a lock.
void writeValue(const char *label, uintptr_t value) {
    char line[96];
    unsigned n = 0;
    while (*label && n < 64) line[n++] = *label++;
    line[n++] = '0'; line[n++] = 'x';
    bool started = false;
    for (int bit = int(sizeof(value) * 8) - 4; bit >= 0; bit -= 4) {
        unsigned digit = (value >> bit) & 15;
        if (digit || started || bit == 0) {
            line[n++] = "0123456789abcdef"[digit];
            started = true;
        }
    }
    line[n++] = '\n';
    (void)write(STDERR_FILENO, line, n);
}

void reportSignal(int number, siginfo_t *info, void *context) {
    if (reporting) _exit(128 + number);
    reporting = 1;
    writeValue("iOS crash signal=", uintptr_t(number));
    writeValue("iOS crash fault=", reinterpret_cast<uintptr_t>(info ? info->si_addr : nullptr));
    auto *registers = static_cast<ucontext_t *>(context);
    if (registers) {
#if defined(__APPLE__) && defined(__aarch64__)
        if (registers->uc_mcontext) {
            writeValue("iOS crash pc=", uintptr_t(arm_thread_state64_get_pc(registers->uc_mcontext->__ss)));
            writeValue("iOS crash lr=", uintptr_t(arm_thread_state64_get_lr(registers->uc_mcontext->__ss)));
        }
#elif defined(__linux__) && defined(__x86_64__)
        writeValue("iOS crash pc=", uintptr_t(registers->uc_mcontext.gregs[REG_RIP]));
#endif
    }
    // Preserve termination by the original signal and the system crash report.
    struct sigaction action = {};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    sigaction(number, &action, nullptr);
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, number);
    sigprocmask(SIG_UNBLOCK, &signals, nullptr);
    kill(getpid(), number);
    _exit(128 + number);
}
}

void InstallClientModCrashDiagnostics() {
    struct sigaction action = {};
    action.sa_sigaction = reportSignal;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    for (int number : {SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE})
        sigaction(number, &action, nullptr);
}
