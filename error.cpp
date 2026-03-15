// error.cpp. Handles error display mode and related utilities.
#include <string>
#include <fstream>
#include "mode_messages.h"

namespace {
    std:: string s_lastErrorReason;

    void log_error(const std::string& source, const std::string& message, const std::string& logPath) {
        std::ofstream log(logPath, std::ios::app);
        if (log) {
            log << "------------------------------" << std::endl;
            log << "ERROR in " << source << ": " << message << std::endl;
            log << "------------------------------" << std::endl;
        }
    }
}

UiRequest error_set_and_log(const std::string& source, const std::string& message, const Context& ctx) {
    s_lastErrorReason = source + ": " + message;
    log_error(source, message, ctx.logPath);
    return UiRequest::MakeNavigate(Mode::Error, s_lastErrorReason);
}

UiRequest error_display(const Context& ctx) {
    std::string body = s_lastErrorReason.empty() ? "An error occurred." : s_lastErrorReason;
    body += "\n\nPress \"Restore now\" to undo any demo changes and clean up.";
    return UiRequest::MakeMessage( "Error", body, "Restore now" );
}