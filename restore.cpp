#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include "mode_messages.h"

void xorFiles(const Context& ctx, AppState& state); // XOR encryption means decryption is the same operation, so we can reuse the function for both steps

UiRequest restoreStart(const Context& ctx, AppState& state) {
    xorFiles(ctx, state); // XOR again to restore original files
    state.restoreInitialized = true;
    std::ofstream log(ctx.logPath, std::ios::app);
    log << "------------------------------" << std::endl;
    log << "Restore Mode: Restored original files by XORing demo copies again." << std::endl;
    log << "------------------------------" << std::endl;

    return UiRequest::MakeMessage(
        "Restore Complete", 
        "Demo files have been restored to their original state. Feel free to check your Downloads directory to see that the copies are now back to their original form. Press Next to remove demo copies and end execution.",
        "Next"
    );
}

UiRequest restoreStep(const Context& ctx, AppState& state) {
    std::ofstream log(ctx.logPath, std::ios::app);
    log << "------------------------------" << std::endl;
    log << "Restore Mode: Removing demo copies." << std::endl;
    log << "------------------------------" << std::endl;

    for (const auto& copyFile : state.copyFiles) {
        std::error_code remove_ec;
        fs::remove(copyFile, remove_ec);
        if (remove_ec) {
            log << "Failed to remove demo file: " << copyFile << ". Error: " << remove_ec.message() << std::endl;
        } else {
            log << "Removed demo file: " << copyFile << std::endl;
        }
    }

    return UiRequest::MakeNavigate(Mode::Exit, "Demo copies removed. Exiting application.");
}

UiRequest run_restore(const Context& ctx, AppState& state) {
    if (!state.restoreInitialized) {
        return restoreStart(ctx, state);
    } else {
        return restoreStep(ctx, state);
    }
}