// educate.cpp
#include "mode_messages.h"

#include <string>
#include <vector>

/*
Duck Plague — educate.cpp (Education Mode)

ROLE
  - Interactive, step-driven safety course.
  - Produces "what to display next" as UiRequest objects (pages + quizzes).
  - Consumes user actions as UserInput (Next click, choice selection, etc.).
  - When complete, signals controller to transition to Restore/Recovery.

CONTRACT (no Qt)
  - Does NOT include Qt headers or manipulate widgets.
  - Exposes:
      UiRequest educate_start(Context& ctx);
      UiRequest educate_handle_input(Context& ctx, const UserInput& input);

INTERNAL MODEL
  - A small state machine / index through a vector of lesson steps.
  - Steps are either:
      - Page: title + body + primary button label
      - Quiz: question + choices + correct answer + feedback

SAFETY / CONTENT BOUNDARIES
  - Course content should be conceptual + defensive:
      - What trojans/ransomware are (high level)
      - What Duck Plague did (copies + hidden originals)
      - How real ransomware differs (no recovery without keys, etc.)
      - Real-world safe response steps (disconnect, backups, get help)
  - Do NOT include "how to evade AV" or operational malware instructions.

HOW TO EXTEND
  - Add pages/quizzes by appending steps to the lesson vector.
  - Add branching by changing next-step logic based on quiz answers.
*/


namespace {
    struct Step {
        enum class Kind { Page, Quiz } kind;
        UiMessage page;
        UiQuiz quiz;
    };

    std::vector<Step> buildLesson() {
        std::vector<Step> steps;

        steps.push_back(Step{
            Step::Kind::Page,
            UiMessage{
                "Duck Plague: Safety Course",
                "This is an educational simulation. No real user data was permanently damaged.\n\n"
                "In the demo, the program created COPIES of files and altered those copies to *look* 'encrypted'. "
                "Original files were only made hidden (a reversible setting).",
                "Next"
            },
            {}
        });

        steps.push_back(Step{
            Step::Kind::Page,
            UiMessage{
                "Trojans (high-level)",
                "A trojan is software that appears to be one thing, but contains hidden behavior.\n\n"
                "Common idea: the user runs it willingly because it looks harmless or useful.\n\n"
                "Defensive takeaway: download from trusted sources, verify signatures when possible, "
                "and be skeptical of unexpected installers.",
                "Next"
            },
            {}
        });

        steps.push_back(Step{
            Step::Kind::Page,
            UiMessage{
                "Ransomware (high-level)",
                "Ransomware is malware that denies access to data and demands something in return.\n\n"
                "In real attacks, data is often encrypted with strong cryptography. Without a key, recovery can be difficult or impossible.\n\n"
                "Defensive takeaway: backups, patching, least privilege, and cautious downloads matter more than 'hoping antivirus catches it.'",
                "Next"
            },
            {}
        });

        steps.push_back(Step{
            Step::Kind::Quiz,
            {},
            UiQuiz{
                "Quick Check",
                "Which of these is the MOST reliable protection against ransomware data loss?",
                {"Paying the ransom", "Having offline backups", "Turning your brightness down", "Renaming files"},
                1,
                "Correct. Offline (or otherwise protected) backups are a top defense against data loss.",
                "Not quite. Backups are what let you restore data without paying or trusting the attacker."
            }
        });

        steps.push_back(Step{
            Step::Kind::Page,
            UiMessage{
                "How Duck Plague differs from real malware",
                "Duck Plague intentionally avoids harmful behavior:\n\n"
                "• No destructive changes to original files (copies only)\n"
                "• No stealth or antivirus-evasion behavior\n"
                "• No persistence mechanisms\n"
                "• No network communication\n\n"
                "The goal is to teach the *impact* and the *psychology* safely.",
                "Next"
            },
            {}
        });

        steps.push_back(Step{
            Step::Kind::Quiz,
            {},
            UiQuiz{
                "Quick Check",
                "If you suspect real ransomware on a machine, what is a good FIRST response?",
                {"Disconnect from networks and get help", "Immediately delete random system files", "Ignore it and hope it stops", "Post screenshots of everything publicly"},
                0,
                "Correct. Reduce spread and get proper support. Preserve evidence if needed.",
                "Not quite. The first goal is to limit damage/spread and get help—avoid making it worse."
            }
        });

        steps.push_back(Step{
            Step::Kind::Page,
            UiMessage{
                "Next: Recovery",
                "You’ve completed the course.\n\n"
                "Next, Duck Plague will restore your system state by:\n"
                "• showing (demonstrating) how decryption would work on demo copies\n"
                "• unhiding originals\n"
                "• deleting demo copies\n\n"
                "Then it returns you to the normal system state.",
                "Continue"
            },
            {}
        });

        return steps;
    }
}

class Educator {
public:
    Educator() : steps_(buildLesson()) {}

    UiRequest start() {
        index_ = 0;
        awaitingQuizAnswer_ = false;
        return currentRequest();
    }

    UiRequest handleInput(const UserInput& input) {
        if (steps_.empty()) {
            return UiRequest::MakeNavigate(Mode::Restore, "No lesson steps; continuing to recovery.");
        }

        const Step& step = steps_[index_];

        if (step.kind == Step::Kind::Page) {
            if (input.kind == InputKind::PrimaryButton) {
                advance();
                return currentRequest();
            }
            // Ignore unexpected input types
            return currentRequest();
        }

        // Quiz step
        if (step.kind == Step::Kind::Quiz) {
            // If we haven't answered yet, require a choice
            if (!awaitingQuizAnswer_) {
                if (input.kind == InputKind::ChoiceSelected) {
                    lastQuizWasCorrect_ = (input.choiceIndex == step.quiz.correctIndex);
                    awaitingQuizAnswer_ = true;

                    // Turn feedback into a "message page" with Next button
                    return UiRequest::MakeMessage(
                        "Quiz Feedback",
                        lastQuizWasCorrect_ ? step.quiz.correctFeedback : step.quiz.incorrectFeedback,
                        "Next"
                    );
                }
                // Still waiting for a choice
                return UiRequest::MakeQuiz(step.quiz);
            }

            // After feedback message, Next advances
            if (input.kind == InputKind::PrimaryButton) {
                awaitingQuizAnswer_ = false;
                advance();
                return currentRequest();
            }

            return UiRequest::MakeMessage("Quiz Feedback", "Click Next to continue.", "Next");
        }

        return currentRequest();
    }

private:
    UiRequest currentRequest() {
        if (index_ >= steps_.size()) {
            return UiRequest::MakeNavigate(Mode::Restore, "Education complete.");
        }

        const Step& step = steps_[index_];
        if (step.kind == Step::Kind::Page) {
            return UiRequest::MakeMessage(step.page.title, step.page.body, step.page.primaryButtonText);
        }

        // Quiz step
        if (!awaitingQuizAnswer_) {
            return UiRequest::MakeQuiz(step.quiz);
        }

        // If already answered, controller should be showing feedback message
        return UiRequest::MakeMessage("Quiz Feedback", "Click Next to continue.", "Next");
    }

    void advance() {
        if (index_ + 1 < steps_.size()) {
            index_++;
        } else {
            index_ = steps_.size(); // marks finished
        }
    }

    std::vector<Step> steps_;
    size_t index_ = 0;
    bool awaitingQuizAnswer_ = false;
    bool lastQuizWasCorrect_ = false;
};

// Exported functions (so controller can use it without knowing the class)
static Educator g_educator;

UiRequest educate_start() {
    return g_educator.start();
}

UiRequest educate_handle_input(const UserInput& input) {
    return g_educator.handleInput(input);
}
