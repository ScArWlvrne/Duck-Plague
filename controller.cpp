#include <QCoreApplication>
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QTimer>
#include <QFrame>
#include <QString>
#include <QRandomGenerator>
#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <functional>
#include "mode_messages.h"

/*
Duck Plague — controller.cpp

ROLE
  - Owns the Qt application + main window.
  - Renders UI for all modes (buttons, text, quiz prompts, progress).
  - Decides mode transitions (state machine / dispatcher).

ARCHITECTURAL RULES
  - This is the ONLY file that includes/uses Qt Widgets directly.
  - No other module (trojan/encrypt/educate/restore/error) should include Qt headers.
  - Modes communicate with the controller via plain C++ structs:
      - UiRequest (interactive modes)
      - ModeResult (worker/run-to-completion modes)
      - Context (shared state/config)

UI MODEL (recommended)
  - Use a QStackedWidget with pages:
      (0) Home page: mode buttons
      (1) Message page: title/body + primary button (Next/Back)
      (2) Quiz page: question + choice buttons 
      (3) Calculator page: number pad + display (Trojan mode)

CONTROLLER RESPONSIBILITIES
  - Create/own Context (downloads path, size limit, demo suffix, log path, etc.).
  - When a mode is entered:
      - For interactive modes (Trojan/Educate): call *_start(ctx), render UiRequest,
        then send UserInput back via *_handle_input(ctx, input) as the user interacts.
      - For worker modes (Encrypt/Restore): call *_run(ctx) and render result;
        later move these to a worker thread to avoid freezing UI.
  - On startup: if demo artifacts are detected (e.g., demo suffix), jump to Restore.

HOW TO EXTEND
  - Add a new mode:
      1) Add new Mode enum value.
      2) Add a button on the Home page.
      3) Wire button -> enterMode(Mode::X).
      4) Implement module function(s) and handle its UiRequest/ModeResult.
  - Add a new UI request type:
      1) Extend UiKind + UiRequest union/struct.
      2) Add a render function (renderX()) in controller.cpp.
      3) Update dispatcher that renders based on UiKind.
*/

// Forward declarations for mode entry points (implemented in other .cpp files).
UiRequest run_trojan(const Context& ctx, AppState& state);
UiRequest trojan_handle_input(const Context& ctx, AppState& state, const UserInput& input);
UiRequest encrypt_start(const Context& ctx, AppState& state);
UiRequest encrypt_step(const Context& ctx, AppState& state, const UserInput& input);
UiRequest run_restore(const Context& ctx, AppState& state);
UiRequest educate_start();


UiRequest educate_handle_input(const UserInput& input);

static bool tryParseEncryptionKeyLine(const std::string& line, uint64_t& key) {
    const std::string prefix = "ENCRYPTION_KEY=";
    if (line.rfind(prefix, 0) != 0) return false;

    std::string value = line.substr(prefix.size());
    try {
        size_t idx = 0;
        int base = 10;
        if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
            base = 16;
        }
        unsigned long long v = std::stoull(value, &idx, base);
        if (idx == 0) return false;
        key = static_cast<uint64_t>(v);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void loadOrGenerateEncryptionKey(const std::string& logPath, AppState& state) {
    std::ifstream in(logPath);
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            uint64_t key;
            if (tryParseEncryptionKeyLine(line, key)) {
                state.encryptionKey = key;
                return;
            }
        }
    }

    state.encryptionKey = QRandomGenerator::global()->generate64();
    std::ofstream out(logPath, std::ios::app);
    if (out) {
        out << "ENCRYPTION_KEY=0x" << std::hex << state.encryptionKey << std::dec << std::endl;
    }
}

void getContext(Context& ctx) {
    namespace fs = std::filesystem;

    // ---- Constants local to the controller (easy to change later) ----
    constexpr size_t DEFAULT_SIZE_LIMIT_MB = 256;
    const std::string DEMO_SUFFIX = "-DEMO";
    const std::string LOG_FILENAME = "duck_plague.log";

    // ---- Downloads path ----
    // Prefer the user's home directory env var, then append "Downloads".
    if (ctx.downloadsPath.empty()) {
        #if defined(_WIN32)
                // Windows: USERPROFILE is usually like C:\Users\<name>
                const char* home = std::getenv("USERPROFILE");
                // Fallbacks (less common)
                if (!home) home = std::getenv("HOMEPATH");
        #else
                // macOS/Linux: HOME is usually like /Users/<name> or /home/<name>
                const char* home = std::getenv("HOME");
        #endif

        if (home) {
            fs::path downloads = fs::path(home) / "Downloads";
            ctx.downloadsPath = downloads.string();
        }
    }

    // ---- Size limit ----
    if (ctx.sizeLimitMB == 0) {
        ctx.sizeLimitMB = DEFAULT_SIZE_LIMIT_MB;
    }

    // ---- Demo suffix ----
    if (ctx.demoSuffix.empty()) {
        ctx.demoSuffix = DEMO_SUFFIX;
    }

    // ---- Log path ----
    // Store logs next to the executable so they're easy to find.
    if (ctx.logPath.empty()) {
        fs::path exeDir = QCoreApplication::applicationDirPath().toStdString();
        fs::path logPath = exeDir / LOG_FILENAME;
        ctx.logPath = logPath.string();

        // Ensure the log file exists
        std::ofstream out(ctx.logPath, std::ios::app);
    }
}

struct HomeWidgets {
    QWidget* page = nullptr;
    QLabel* label = nullptr;
    QPushButton* trojanBtn = nullptr;
    QPushButton* encryptBtn = nullptr;
    QPushButton* educateBtn = nullptr;
    QPushButton* restoreBtn = nullptr;
    QPushButton* errorBtn = nullptr;
};

struct ModeWidgets {
    QWidget* page = nullptr;
    QLabel* titleLabel = nullptr;
    QLabel* bodyLabel = nullptr;
    QPushButton* primaryBtn = nullptr; // Next / Proceed (set per UiRequest)
    QPushButton* backBtn = nullptr;
};

// Dedicated quiz page — keeps quiz UI separate from message UI.
struct QuizWidgets {
    QWidget* page = nullptr;
    QLabel* questionLabel = nullptr;
    QPushButton* choiceButtons[4] = {nullptr, nullptr, nullptr, nullptr};
    QPushButton* backBtn = nullptr;
};

struct CalcWidgets {
    QWidget* page         = nullptr;
    QLabel*  displayLabel = nullptr;
};

// Page builders

// Builds the Home page (label + mode buttons) and adds it to the stack.
HomeWidgets buildHomePage(QStackedWidget* stack) {
    HomeWidgets hw;

    hw.page = new QWidget();
    auto* homeLayout = new QVBoxLayout(hw.page);

    hw.label = new QLabel("Controller: Home Screen");

    hw.trojanBtn  = new QPushButton("Enter Trojan Mode");
    hw.encryptBtn = new QPushButton("Enter Encrypt Mode");
    hw.educateBtn = new QPushButton("Enter Education Mode");
    hw.restoreBtn = new QPushButton("Enter Restore Mode");
    hw.errorBtn   = new QPushButton("Enter Error Mode");

    homeLayout->addWidget(hw.label);
    homeLayout->addWidget(hw.trojanBtn);
    homeLayout->addWidget(hw.encryptBtn);
    homeLayout->addWidget(hw.educateBtn);
    homeLayout->addWidget(hw.restoreBtn);
    homeLayout->addWidget(hw.errorBtn);

    stack->addWidget(hw.page); // index 0 (first page added)

    return hw;
}

// Builds the Mode/Message page (title/body + Next/Back buttons).
ModeWidgets buildModePage(QStackedWidget* stack) {
    ModeWidgets mw;

    mw.page = new QWidget();
    auto* layout = new QVBoxLayout(mw.page);

    mw.titleLabel = new QLabel("Mode Screen");
    mw.titleLabel->setWordWrap(true);
    mw.titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; margin-bottom: 10px;");

    mw.bodyLabel = new QLabel("(no content yet)");
    mw.bodyLabel->setWordWrap(true);
    mw.bodyLabel->setStyleSheet("font-size: 14px; margin-bottom: 20px;");

    mw.primaryBtn = new QPushButton("Next");
    mw.primaryBtn->setMinimumHeight(40);

    mw.backBtn = new QPushButton("Back to Controller");
    mw.backBtn->setFlat(true);
    mw.backBtn->setStyleSheet("color: gray; font-size: 11px;");

    layout->addWidget(mw.titleLabel);
    layout->addWidget(mw.bodyLabel);
    layout->addStretch();
    layout->addWidget(mw.primaryBtn);

    // Back button centered at the bottom
    auto* exitLayout = new QHBoxLayout();
    exitLayout->addStretch();
    exitLayout->addWidget(mw.backBtn);
    exitLayout->addStretch();
    layout->addLayout(exitLayout);

    stack->addWidget(mw.page); // index 1 (second page added)

    return mw;
}


// Builds the Quiz page — teammate's design: question label + 4 choice buttons + Exit.
QuizWidgets buildQuizPage(QStackedWidget* stack) {
    QuizWidgets qw;

    qw.page = new QWidget();
    auto* layout = new QVBoxLayout(qw.page);
    layout->setSpacing(20);
    layout->setContentsMargins(40, 40, 40, 40);

    qw.questionLabel = new QLabel("(Question will appear here)");
    qw.questionLabel->setWordWrap(true);
    qw.questionLabel->setAlignment(Qt::AlignCenter);
    qw.questionLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    layout->addWidget(qw.questionLabel);

    for (int i = 0; i < 4; ++i) {
        qw.choiceButtons[i] = new QPushButton();
        qw.choiceButtons[i]->setMinimumHeight(50);
        layout->addWidget(qw.choiceButtons[i]);
    }

    layout->addStretch();

    qw.backBtn = new QPushButton("Exit Quiz");
    qw.backBtn->setFlat(true);
    qw.backBtn->setStyleSheet("color: gray; font-size: 11px;");

    auto* exitLayout = new QHBoxLayout();
    exitLayout->addStretch();
    exitLayout->addWidget(qw.backBtn);
    exitLayout->addStretch();
    layout->addLayout(exitLayout);

    stack->addWidget(qw.page); // index 2 (third page added)

    return qw;
}

// builds a basic calculator -- TBD can change styles later, currently functional

CalcWidgets buildCalcPage(QStackedWidget* stack,
                           const std::function<void(const std::string&)>& onButton) {
    CalcWidgets cw;
    cw.page = new QWidget();
    auto* outerLayout = new QVBoxLayout(cw.page);

    // ---- Display ----
    cw.displayLabel = new QLabel("0");
    cw.displayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cw.displayLabel->setMinimumHeight(40);
    cw.displayLabel->setFrameShape(QFrame::StyledPanel);
    cw.displayLabel->setFrameShadow(QFrame::Sunken);
    QFont df = cw.displayLabel->font();
    df.setPointSize(14);
    cw.displayLabel->setFont(df);
    outerLayout->addWidget(cw.displayLabel);


    auto* grid = new QGridLayout();
    grid->setSpacing(4);

    struct BtnSpec { int row; int col; int rspan; int cspan; const char* label; };
    const BtnSpec specs[] = {
        {0, 0, 1, 1, "C"}, {0, 3, 1, 1, "/"},
        {1, 0, 1, 1, "7"}, {1, 1, 1, 1, "8"}, {1, 2, 1, 1, "9"}, {1, 3, 1, 1, "*"},
        {2, 0, 1, 1, "4"}, {2, 1, 1, 1, "5"}, {2, 2, 1, 1, "6"}, {2, 3, 1, 1, "-"},
        {3, 0, 1, 1, "1"}, {3, 1, 1, 1, "2"}, {3, 2, 1, 1, "3"}, {3, 3, 1, 1, "+"},
        {4, 0, 1, 2, "0"}, {4, 2, 1, 1, "."}, {4, 3, 1, 1, "="},
    };

    for (const auto& s : specs) {
        auto* btn = new QPushButton(QString::fromUtf8(s.label));
        btn->setMinimumHeight(40);
        std::string lbl(s.label);
        QObject::connect(btn, &QPushButton::clicked, [lbl, onButton]() {
            onButton(lbl);
        });
        grid->addWidget(btn, s.row, s.col, s.rspan, s.cspan);
    }
    for (int c = 0; c < 4; ++c) grid->setColumnStretch(c, 1);

    outerLayout->addLayout(grid);
    stack->addWidget(cw.page); // index 3 (fourth page added)
    return cw;
}

// mode runs

UiRequest runMode(Mode mode, const Context& ctx, AppState& state) {
    switch (mode) {
        case Mode::Trojan:
            return run_trojan(ctx, state);
        case Mode::Encrypt:
            state.encryptInitialized = true;
            return encrypt_start(ctx, state);
        case Mode::Educate:
            //return UiRequest::MakeMessage("Education Mode (Stub)", "Educate module not implemented yet.");
            return educate_start();
        case Mode::Restore:
            //return UiRequest::MakeMessage("Restore Mode (Stub)", "Restore module not implemented yet.");
            return run_restore(ctx, state);
        case Mode::Error:
            return UiRequest::MakeMessage("Error Mode (Stub)", "Error module not implemented yet.");
        case Mode::Controller:
            return UiRequest::MakeMessage("Controller", "Already on the controller home screen.");
        case Mode::Exit:
            return UiRequest::MakeMessage("Exit", "Exit requested.");
        default:
            return UiRequest::MakeMessage("Unknown", "Unknown mode.");
    }
}


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;                       // A blank window
    window.setWindowTitle("Duck Plague"); // Window title bar text

    // Set a reasonable default window size
    window.resize(800, 600);
    window.setMinimumSize(600, 400);

    // We'll use a QStackedWidget so we can switch between "pages" (Home, Mode screens, etc.).
    auto *outerLayout = new QVBoxLayout(&window);
    auto *stack = new QStackedWidget();
    outerLayout->addWidget(stack);

    // Build all pages
    HomeWidgets home     = buildHomePage(stack); // index 0
    ModeWidgets modePage = buildModePage(stack); // index 1
    QuizWidgets quizPage = buildQuizPage(stack); // index 2  

    Context ctx{};
    getContext(ctx);

    AppState state{};
    loadOrGenerateEncryptionKey(ctx.logPath, state);

    // mode to track currentMode - used for education module and mode-specific button wiring
    Mode activeMode = Mode::Controller; // starts at home/controller

    // calculator page in trojan mode
    // renderMessage is declared as std::function so buildCalcPage callbacks
    // can reference it before the lambda is fully defined.
    std::function<void(const UiRequest&)> renderMessage;

    auto onCalcButton = [&](const std::string& label) {
        if (activeMode != Mode::Trojan) return;
        UserInput inp;
        inp.kind       = InputKind::CalcButtonPress;
        inp.buttonText = label;
        renderMessage(trojan_handle_input(ctx, state, inp));
    };

    CalcWidgets calcPage = buildCalcPage(stack, onCalcButton); // index 3

    // ---- 1-second timer — only active in Trojan mode ----
    auto* calcTimer = new QTimer(&window);
    calcTimer->setInterval(1000);
    QObject::connect(calcTimer, &QTimer::timeout, [&]() {
        if (activeMode != Mode::Trojan) return;
        UserInput inp;
        inp.kind = InputKind::Tick;
        renderMessage(trojan_handle_input(ctx, state, inp));
    });
    calcTimer->start();

    // renders quiz questions
    auto renderQuiz = [&](const UiRequest& req) {
        quizPage.questionLabel->setText(QString::fromStdString(req.quiz.question));

        for (int i = 0; i < 4; ++i) {
            if (i < static_cast<int>(req.quiz.choices.size())) {
                quizPage.choiceButtons[i]->setText(
                    QString::fromStdString(req.quiz.choices[i]));
                quizPage.choiceButtons[i]->setVisible(true);
            } else {
                quizPage.choiceButtons[i]->setVisible(false);
            }
        }

        stack->setCurrentWidget(quizPage.page);
    };

    // Handles Calculator, Navigate, Quiz, and Message UiKinds.
    renderMessage = [&](const UiRequest& req) {
        if (req.kind == UiKind::Calculator) {
            // Update calculator display and ensure calc page is shown.
            calcPage.displayLabel->setText(
                QString::fromStdString(req.calculator.displayText));
            stack->setCurrentWidget(calcPage.page);
            return;
        }

        if (req.kind == UiKind::Navigate) {
            // Transition to the requested mode.
            activeMode = req.nav.nextMode;
            if (activeMode == Mode::Controller) {
                stack->setCurrentWidget(home.page);
                return;
            }
            if (activeMode == Mode::Exit) {
                // Cleanly shut down the Qt event loop - app.exec() returns.
                QCoreApplication::quit();
                return;
            }
            // Run the new mode and render its first request recursively.
            renderMessage(runMode(activeMode, ctx, state));
            return;
        }

        if (req.kind == UiKind::Quiz) {
            // Show the dedicated quiz page (teammate's design).
            renderQuiz(req);
            return;
        }

        // Message: lesson page, feedback, encrypt status, etc.
        modePage.titleLabel->setText(QString::fromStdString(req.message.title));
        modePage.bodyLabel->setText(QString::fromStdString(req.message.body));

        const std::string btnText = req.message.primaryButtonText;
        if (btnText.empty()) {
            modePage.primaryBtn->hide();
        } else {
            modePage.primaryBtn->setText(QString::fromStdString(btnText));
            modePage.primaryBtn->show();
            modePage.primaryBtn->setEnabled(true);
        }
        stack->setCurrentWidget(modePage.page);
    };

    // handles quiz button choices
    auto handleChoiceClick = [&](int choiceIndex) {
        UserInput input;
        input.kind        = InputKind::ChoiceSelected;
        input.choiceIndex = choiceIndex;
        UiRequest req = educate_handle_input(input);
        renderMessage(req);
    };

    // Wire the 4 quiz choice buttons
    for (int i = 0; i < 4; ++i) {
        QObject::connect(quizPage.choiceButtons[i], &QPushButton::clicked, [&, i]() {
            handleChoiceClick(i);
        });
    }

    auto connectModeButton = [&](QPushButton* btn, Mode m) {
        // IMPORTANT: capture `m` by value so each button keeps its own mode.
        QObject::connect(btn, &QPushButton::clicked, [&, m]() {
            activeMode = m;
            if (m == Mode::Trojan) {
                // Initialize/reset calculator state, then show the calc page.
                UiRequest req = run_trojan(ctx, state);
                calcPage.displayLabel->setText(
                    QString::fromStdString(req.calculator.displayText));
                stack->setCurrentWidget(calcPage.page);
                return;
            }
            renderMessage(runMode(m, ctx, state));
        });
    };

    connectModeButton(home.trojanBtn,  Mode::Trojan);
    connectModeButton(home.encryptBtn, Mode::Encrypt);
    connectModeButton(home.educateBtn, Mode::Educate);
    connectModeButton(home.restoreBtn, Mode::Restore);
    connectModeButton(home.errorBtn,   Mode::Error);

    //Primary "Next" button — handles Encrypt, Educate, and Restore step-by-step 
    QObject::connect(modePage.primaryBtn, &QPushButton::clicked, [&]() {
        if (activeMode == Mode::Encrypt) {
            UserInput input{};
            input.kind = InputKind::PrimaryButton;
            renderMessage(encrypt_step(ctx, state, input));
            stack->setCurrentWidget(modePage.page);
        } else if (activeMode == Mode::Educate) {
            UserInput input{};
            input.kind = InputKind::PrimaryButton;
            renderMessage(educate_handle_input(input));
        } else if (activeMode == Mode::Restore) {
            // restoreInitialized is already true at this point (set by restoreStart),
            // so run_restore() will call restoreStep() → Navigate(Exit).
            renderMessage(run_restore(ctx, state));
        }
    });


    // back / exit buttons
    auto handleExit = [&]() {
        activeMode = Mode::Controller;
        stack->setCurrentWidget(home.page); // back to Home page
    };

    QObject::connect(modePage.backBtn, &QPushButton::clicked, handleExit);
    QObject::connect(quizPage.backBtn, &QPushButton::clicked, handleExit);

    window.show();
    return app.exec();
}