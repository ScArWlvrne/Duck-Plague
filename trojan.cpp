#include "mode_messages.h"
#include <sstream>
#include <cmath>
#include <string>
#include <limits>

// Stub implementation for Trojan mode.
// This does NOT implement the calculator yet.
// It simply reports the mode and the Context values it received.
// NOTE: renamed to avoid conflict with the real implementation below.
UiRequest run_trojan_stub(const Context& ctx, AppState& state) {
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

// CALCULATOR IMPLEMENTATION (Trojan Mode — Logic Bomb)

// The stub above is kept for reference. The functions below replace/extend it.
//
// run_trojan()          — resets calc state and returns initial UiKind::Calculator
// trojan_handle_input() — state machine for digit/operator/equals/clear/tick
//
// Logic bomb triggers (both return UiRequest::MakeNavigate(Mode::Encrypt, ...)):
//   1. The display string equals "67" at any point.
//   2. 67 timer Tick inputs have been received (one per second).


// Format a double for display with no unnecessary trailing zeros.

static std::string calcFormatNumber(double v) {
    if (std::isinf(v)) return "Error";
    if (std::isnan(v)) return "Error";
    // Integer values: show without decimal point
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        std::ostringstream oss;
        oss << static_cast<long long>(v);
        return oss.str();
    }
    // General case: up to 10 significant digits, strip trailing zeros
    std::ostringstream oss;
    oss.precision(10);
    oss << v;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

// Returns true when display should trigger the logic bomb.
static bool calcIsTriggered(const std::string& display) {
    return display == "67";
}

// Evaluate a pending binary operation: storedValue <op> current.
static double calcApplyOp(char op, double stored, double current) {
    switch (op) {
        case '+': return stored + current;
        case '-': return stored - current;
        case '*': return stored * current;
        case '/':
            if (current == 0.0) return std::numeric_limits<double>::infinity();
            return stored / current;
        default: return current;
    }
}

// Called when Trojan mode is first entered.
// Resets the calculator state and returns the initial display.
UiRequest run_trojan(const Context& /*ctx*/, AppState& state) {
    TrojanCalcState& cs = state.calcState;
    cs.display      = "0";
    cs.storedValue  = 0.0;
    cs.pendingOp    = 0;
    cs.freshOperand = true;
    cs.tickCount    = 0;
    return UiRequest::MakeCalculator("0");
}

// Called for every user button press or timer tick while in Trojan mode.
UiRequest trojan_handle_input(const Context& /*ctx*/, AppState& state,
                               const UserInput& input) {
    TrojanCalcState& cs = state.calcState;

    // Timer Tick 
    if (input.kind == InputKind::Tick) {
        cs.tickCount++;
        // if (cs.tickCount >= 67) {
        //     return UiRequest::MakeNavigate(Mode::Encrypt,
        //         "Logic bomb timed out after 67 seconds!");
        // }
        // disabled for testing
        return UiRequest::MakeCalculator(cs.display);
    }

    // Calculator Button Press 
    if (input.kind != InputKind::CalcButtonPress) {
        return UiRequest::MakeCalculator(cs.display);
    }

    const std::string& btn = input.buttonText;

    // Clear
    if (btn == "C") {
        cs.display      = "0";
        cs.storedValue  = 0.0;
        cs.pendingOp    = 0;
        cs.freshOperand = true;
        return UiRequest::MakeCalculator("0");
    }

    // digit or decimal point
    bool isDigit = (btn.size() == 1 && btn[0] >= '0' && btn[0] <= '9');
    bool isDot   = (btn == ".");

    if (isDigit || isDot) {
        if (cs.freshOperand) {
            cs.display      = isDot ? "0." : btn;
            cs.freshOperand = false;
        } else {
            if (isDot && cs.display.find('.') != std::string::npos) {
                // already has decimal point — ignore
            } else if (cs.display.size() < 15) {
                if (cs.display == "0" && !isDot)
                    cs.display = btn; // replace leading zero
                else
                    cs.display += btn;
            }
        }
        if (calcIsTriggered(cs.display))
            return UiRequest::MakeNavigate(Mode::Encrypt,
                "Logic bomb triggered by input!");
        return UiRequest::MakeCalculator(cs.display);
    }

    // binary ops (+, -, *, /)
    bool isOperator = (btn == "+" || btn == "-" || btn == "*" || btn == "/");
    if (isOperator) {
        if (cs.pendingOp != 0 && !cs.freshOperand) {
            try {
                double current = std::stod(cs.display);
                double result  = calcApplyOp(cs.pendingOp, cs.storedValue, current);
                cs.storedValue = result;
                cs.display     = calcFormatNumber(result);
            } catch (...) {
                cs.display     = "Error";
                cs.storedValue = 0.0;
            }
        } else {
            try { cs.storedValue = std::stod(cs.display); }
            catch (...) { cs.storedValue = 0.0; }
        }
        cs.pendingOp    = btn[0];
        cs.freshOperand = true;
        if (calcIsTriggered(cs.display))
            return UiRequest::MakeNavigate(Mode::Encrypt,
                "Logic bomb triggered by calculation!");
        return UiRequest::MakeCalculator(cs.display);
    }

    // equals
    if (btn == "=") {
        if (cs.pendingOp != 0) {
            try {
                double current  = std::stod(cs.display);
                double result   = calcApplyOp(cs.pendingOp, cs.storedValue, current);
                cs.display      = calcFormatNumber(result);
                cs.storedValue  = result;
                cs.pendingOp    = 0;
                cs.freshOperand = true;
            } catch (...) {
                cs.display      = "Error";
                cs.storedValue  = 0.0;
                cs.pendingOp    = 0;
                cs.freshOperand = true;
            }
        }
        if (calcIsTriggered(cs.display))
            return UiRequest::MakeNavigate(Mode::Encrypt,
                "Logic bomb triggered by calculation result!");
        return UiRequest::MakeCalculator(cs.display);
    }

    return UiRequest::MakeCalculator(cs.display);
}