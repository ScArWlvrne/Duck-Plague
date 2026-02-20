#include <QCoreApplication>
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QString>
#include <QRandomGenerator>
#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <fstream>
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
      (1) Home page: mode buttons
      (2) Message page: title/body + primary button (Next/Back)
      (3) Quiz page: question + choice buttons
      (4) Progress page: status text + progress bar (optional later)

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
    QPushButton* backBtn = nullptr;
};

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

// Builds the Mode page (title/body + Back button) and adds it to the stack.
ModeWidgets buildModePage(QStackedWidget* stack) {
    ModeWidgets mw;

    mw.page = new QWidget();
    auto* layout = new QVBoxLayout(mw.page);

    mw.titleLabel = new QLabel("Mode Screen");
    mw.titleLabel->setWordWrap(true);

    mw.bodyLabel = new QLabel("(no content yet)");
    mw.bodyLabel->setWordWrap(true);

    mw.backBtn = new QPushButton("Back to Controller");

    layout->addWidget(mw.titleLabel);
    layout->addWidget(mw.bodyLabel);
    layout->addWidget(mw.backBtn);

    stack->addWidget(mw.page); // index 1 (second page added)

    return mw;
}

UiRequest runMode(Mode mode, const Context& ctx, AppState& state) {
    switch (mode) {
        case Mode::Trojan:
            return run_trojan(ctx, state);
        case Mode::Encrypt:
            return UiRequest::MakeMessage("Encrypt Mode (Stub)", "Encrypt module not implemented yet.");
        case Mode::Educate:
            return UiRequest::MakeMessage("Education Mode (Stub)", "Educate module not implemented yet.");
        case Mode::Restore:
            return UiRequest::MakeMessage("Restore Mode (Stub)", "Restore module not implemented yet.");
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

    // We'll use a QStackedWidget so we can switch between "pages" (Home, Mode screens, etc.).
    auto *outerLayout = new QVBoxLayout(&window);
    auto *stack = new QStackedWidget();
    outerLayout->addWidget(stack);

    // Home page widget (label + mode buttons)
    HomeWidgets home = buildHomePage(stack);
    ModeWidgets modePage = buildModePage(stack);

    Context ctx{};
    getContext(ctx);

    AppState state{};
    loadOrGenerateEncryptionKey(ctx.logPath, state);

    auto renderMessage = [&](const UiRequest& req) {
        // For now we only support Message requests.
        modePage.titleLabel->setText(QString::fromStdString(req.message.title));
        modePage.bodyLabel->setText(QString::fromStdString(req.message.body));
    };

    auto connectModeButton = [&](QPushButton* btn, Mode m) {
        // IMPORTANT: capture `m` by value so each button keeps its own mode.
        QObject::connect(btn, &QPushButton::clicked, [&, m]() {
            renderMessage(runMode(m, ctx, state));
            stack->setCurrentWidget(modePage.page); // switch to Mode page
        });
    };

    connectModeButton(home.trojanBtn,  Mode::Trojan);
    connectModeButton(home.encryptBtn, Mode::Encrypt);
    connectModeButton(home.educateBtn, Mode::Educate);
    connectModeButton(home.restoreBtn, Mode::Restore);
    connectModeButton(home.errorBtn,   Mode::Error);

    QObject::connect(modePage.backBtn, &QPushButton::clicked, [&]() {
        stack->setCurrentWidget(home.page); // back to Home page
    });

    window.show();
    return app.exec();
}