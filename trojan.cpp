#include "mode_messages.h"
#include <sstream>

// Stub implementation for Trojan mode.
// This does NOT implement the calculator yet.
// It simply reports the mode and the Context values it received.

UiRequest run_trojan(const Context& ctx) {
    std::ostringstream body;

    body << "Current mode: Trojan\n\n";
    body << "Context received:\n";
    body << "Downloads path: " << ctx.downloadsPath << "\n";
    body << "Size limit (MB): " << ctx.sizeLimitMB << "\n";
    body << "Demo suffix: " << ctx.demoSuffix << "\n";
    body << "Log path: " << ctx.logPath << "\n";

    return UiRequest::MakeMessage(
        "Trojan Mode (Stub)",
        body.str(),
        "Back to Controller"
    );
}