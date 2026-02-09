// mode_messages.h (shared)
#pragma once
#include <string>
#include <vector>

enum class Mode { Controller, Trojan, Encrypt, Educate, Restore, Error, Exit };

enum class UiKind { Message, Quiz, Navigate };

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

struct UiRequest {
    UiKind kind;
    UiMessage message;
    UiQuiz quiz;
    UiNavigate nav;

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
};

enum class InputKind { PrimaryButton, ChoiceSelected };

struct UserInput {
    InputKind kind;
    int choiceIndex = -1; // used when ChoiceSelected
};
