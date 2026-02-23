// mode_messages.h (shared)
#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

enum class Mode { Controller, Trojan, Encrypt, Educate, Restore, Error, Exit };

enum class UiKind { Message, Quiz, Navigate, Calculator };

struct UiMessage {
    std::string title;
    std::string body;
    std::string primaryButtonText;   // e.g., "Next"
};

struct UiQuiz {
    std::string title;
    std::string question;
    std::vector<std::string> choices;
    int correctIndex;                // simple approach
    std::string correctFeedback;
    std::string incorrectFeedback;
};

struct UiNavigate {
    Mode nextMode;
    std::string reason;              // shown by controller if desired
};

struct UiCalculator {
    std::string displayText;         // what the calculator screen shows
};

struct UiRequest {
    UiKind kind;
    UiMessage message;
    UiQuiz quiz;
    UiNavigate nav;
    UiCalculator calculator;

    static UiRequest MakeMessage(std::string t, std::string b, std::string btn="Next") {
        UiRequest r; r.kind = UiKind::Message;
        r.message = {std::move(t), std::move(b), std::move(btn)};
        return r;
    }

    static UiRequest MakeQuiz(UiQuiz q) {
        UiRequest r; r.kind = UiKind::Quiz;
        r.quiz = std::move(q);
        return r;
    }

    static UiRequest MakeNavigate(Mode next, std::string why) {
        UiRequest r; r.kind = UiKind::Navigate;
        r.nav = {next, std::move(why)};
        return r;
    }

    static UiRequest MakeCalculator(std::string text) {
        UiRequest r; r.kind = UiKind::Calculator;
        r.calculator = {std::move(text)};
        return r;
    }
};

enum class InputKind { PrimaryButton, ChoiceSelected, CalcButtonPress, Tick };

struct UserInput {
    InputKind kind;
    int choiceIndex = -1;       // used when ChoiceSelected
    std::string buttonText;     // used when CalcButtonPress
};

struct Context {
    std::string downloadsPath;
    size_t sizeLimitMB;
    std::string demoSuffix;
    std::string logPath;
};

enum class EncryptPhase {
    Warning,
    Scanning,
    Copying,
    Encrypting,
    Done
};

struct TrojanCalcState {
    std::string display     = "0";  // what is shown on screen
    double      storedValue = 0.0;  // left-hand operand
    char        pendingOp   = 0;    // '+', '-', '*', '/'
    bool        freshOperand = true; // next digit starts a new number
    int         tickCount   = 0;    // seconds elapsed in Trojan mode
};

struct AppState {
    std::vector<fs::path> targetFiles;
    std::vector<fs::path> copyFiles;
    uint64_t encryptionKey;

    EncryptPhase encryptPhase = EncryptPhase::Warning;
    bool encryptInitialized = false;

    bool restoreInitialized = false;

    TrojanCalcState calcState;
};
