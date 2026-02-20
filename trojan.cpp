#include "mode_messages.h"
#include <sstream>

// Stub implementation for Trojan mode.
// This does NOT implement the calculator yet.
// It simply reports the mode and the Context values it received.

UiRequest run_trojan(const Context& ctx, AppState& state) {
    std::ostringstream body;

    body << "Current mode: Trojan\n\n";
    body << "Context received:\n";
    body << "Downloads path: " << ctx.downloadsPath << "\n";
    body << "Size limit (MB): " << ctx.sizeLimitMB << "\n";
    body << "Demo suffix: " << ctx.demoSuffix << "\n";
    body << "Log path: " << ctx.logPath << "\n";
    body << "\nAppState received:\n";
    body << "Encryption key: 0x" << std::hex << state.encryptionKey << std::dec << "\n";
    body << "Number of target files: " << state.targetFiles.size() << "\n";
    body << "Number of copy files: " << state.copyFiles.size() << "\n";

    return UiRequest::MakeMessage(
        "Trojan Mode (Stub)",
        body.str(),
        "Back to Controller"
    );
}