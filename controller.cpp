#include "mode_messages.h"
#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>

/*
Duck Plague — controller.cpp

ROLE
  - Owns the Qt application + main window.
  - Renders UI for all modes (buttons, text, quiz prompts, progress).
  - Decides mode transitions (state machine / dispatcher).

ARCHITECTURAL RULES
  - This is the ONLY file that includes/uses Qt Widgets directly.
  - No other module (trojan/encrypt/educate/restore/error) should include Qt
headers.
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
  - Create/own Context (downloads path, size limit, demo suffix, log path,
etc.).
  - When a mode is entered:
      - For interactive modes (Trojan/Educate): call *_start(ctx), render
UiRequest, then send UserInput back via *_handle_input(ctx, input) as the user
interacts.
      - For worker modes (Encrypt/Restore): call *_run(ctx) and render result;
        later move these to a worker thread to avoid freezing UI.
  - On startup: if demo artifacts are detected (e.g., demo suffix), jump to
Restore.

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
UiRequest run_trojan(const Context &ctx);
UiRequest educate_start();
// run_educate() vs educate_start()

// new below
UiRequest educate_handle_input(const UserInput &input);

void getContext(Context &ctx) {
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
    const char *home = std::getenv("USERPROFILE");
    // Fallbacks (less common)
    if (!home)
      home = std::getenv("HOMEPATH");
#else
    // macOS/Linux: HOME is usually like /Users/<name> or /home/<name>
    const char *home = std::getenv("HOME");
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
  QWidget *page = nullptr;
  QLabel *label = nullptr;
  QPushButton *trojanBtn = nullptr;
  QPushButton *encryptBtn = nullptr;
  QPushButton *educateBtn = nullptr;
  QPushButton *restoreBtn = nullptr;
  QPushButton *errorBtn = nullptr;
};

struct ModeWidgets {
  QWidget *page = nullptr;
  QLabel *titleLabel = nullptr;
  QLabel *bodyLabel = nullptr;
  QPushButton *nextBtn = nullptr;
  QPushButton *backBtn = nullptr;
};

struct QuizWidgets {
  QWidget *page = nullptr;
  QLabel *questionLabel = nullptr;
  QPushButton *choiceButtons[4] = {nullptr, nullptr, nullptr, nullptr};
  QPushButton *nextBtn = nullptr;
  QPushButton *backBtn = nullptr;
};

// Builds the Home page (label + mode buttons) and adds it to the stack.
HomeWidgets buildHomePage(QStackedWidget *stack) {
  HomeWidgets hw;

  hw.page = new QWidget();
  auto *homeLayout = new QVBoxLayout(hw.page);

  hw.label = new QLabel("Controller: Home Screen");

  hw.trojanBtn = new QPushButton("Enter Trojan Mode");
  hw.encryptBtn = new QPushButton("Enter Encrypt Mode");
  hw.educateBtn = new QPushButton("Enter Education Mode");
  hw.restoreBtn = new QPushButton("Enter Restore Mode");
  hw.errorBtn = new QPushButton("Enter Error Mode");

  homeLayout->addWidget(hw.label);
  homeLayout->addWidget(hw.trojanBtn);
  homeLayout->addWidget(hw.encryptBtn);
  homeLayout->addWidget(hw.educateBtn);
  homeLayout->addWidget(hw.restoreBtn);
  homeLayout->addWidget(hw.errorBtn);

  stack->addWidget(hw.page); // index 0 (first page added)

  return hw;
}

QuizWidgets buildQuizPage(QStackedWidget *stack) {
  QuizWidgets qw;

  qw.page = new QWidget();
  auto *layout = new QVBoxLayout(qw.page);

  layout->setSpacing(20);
  layout->setContentsMargins(40, 40, 40, 40);

  // question label for educate
  qw.questionLabel = new QLabel("(Question will appear here)");
  qw.questionLabel->setWordWrap(true);
  qw.questionLabel->setAlignment(Qt::AlignCenter);
  qw.questionLabel->setStyleSheet("font-size: 18px; font-weight: bold;");

  layout->addWidget(qw.questionLabel);

  // 4x choice buttons
  for (int i = 0; i < 4; ++i) {
    qw.choiceButtons[i] = new QPushButton();
    qw.choiceButtons[i]->setMinimumHeight(50);

    layout->addWidget(qw.choiceButtons[i]);
  }
  layout->addStretch();

  //exit button
  qw.backBtn = new QPushButton("Exit Quiz");
  qw.backBtn->setFlat(true);
  qw.backBtn->setStyleSheet("color: gray; font-size: 11px;");

  auto *exitLayout = new QHBoxLayout();
  exitLayout->addStretch();
  exitLayout->addWidget(qw.backBtn);
  exitLayout->addStretch();
  layout->addLayout(exitLayout);

  stack->addWidget(qw.page);

  return qw;
}

// Builds the Mode page (title/body + Back button) and adds it to the stack.
ModeWidgets buildModePage(QStackedWidget *stack) {
  ModeWidgets mw;

  mw.page = new QWidget();
  auto *layout = new QVBoxLayout(mw.page);

  //title
  mw.titleLabel = new QLabel("Mode Screen");
  mw.titleLabel->setWordWrap(true);
  mw.titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; margin-bottom: 10px;");
  
  //body
  mw.bodyLabel = new QLabel("(no content yet)");
  mw.bodyLabel->setWordWrap(true);
  mw.bodyLabel->setStyleSheet("font-size: 14px; margin-bottom: 20px;");

  //next (primary button in quiz so bigger)
  mw.nextBtn = new QPushButton("Next");
  mw.nextBtn->setMinimumHeight(40);

  //back button
  mw.backBtn = new QPushButton("Back to Controller");
  mw.backBtn->setFlat(true);
  mw.backBtn->setStyleSheet("color: gray; font-size: 11px;");

  //layout
  layout->addWidget(mw.titleLabel);
  layout->addWidget(mw.bodyLabel);
  layout->addStretch();

  layout->addWidget(mw.nextBtn);
  //layout->addWidget(mw.backBtn);

  //exit button
  auto *exitLayout = new QHBoxLayout();
  exitLayout->addStretch();
  exitLayout->addWidget(mw.backBtn);
  exitLayout->addStretch();
  layout->addLayout(exitLayout);

  stack->addWidget(mw.page); // index 1 (second page added)

  return mw;
}

UiRequest runMode(Mode mode, const Context &ctx) {
  switch (mode) {
  case Mode::Trojan:
    return run_trojan(ctx);
  case Mode::Encrypt:
    return UiRequest::MakeMessage("Encrypt Mode (Stub)",
                                  "Encrypt module not implemented yet.");
  case Mode::Educate:

    // currently implemented without (ctx) context, so i called with empty ().
    // potentially change in future according to comments in educate.cpp
    // this looks fine though
    return educate_start();
    // below is test: stub before implementation completed
    // return UiRequest::MakeMessage("Education Mode (Stub)",
        // "Educate module not implemented yet.");
  case Mode::Restore:
    return UiRequest::MakeMessage(
        "Restore Mode (Stub)",
        "Restore module not implemented yet.");
case Mode::Error:
    return UiRequest::MakeMessage(
        "Error Mode (Stub)",
        "Error module not implemented yet.");
  case Mode::Controller:
    return UiRequest::MakeMessage(
        "Controller",
        "Already on the controller home screen.");
case Mode::Exit:
    return UiRequest::MakeMessage(
        "Exit", "Exit requested.");
  default:
    return UiRequest::MakeMessage(
        "Unknown", "Unknown mode.");
  }
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QWidget window;                       // A blank window
  window.setWindowTitle("Duck Plague"); // Window title bar text

  // We'll use a QStackedWidget so we can switch between "pages" (Home, Mode
  // screens, etc.).
  auto *outerLayout = new QVBoxLayout(&window);
  auto *stack = new QStackedWidget();
  outerLayout->addWidget(stack);

  // Home page widget (label + mode buttons)
  HomeWidgets home = buildHomePage(stack);
  ModeWidgets modePage = buildModePage(stack);
  QuizWidgets quizPage = buildQuizPage(stack);

  Context ctx{};
  getContext(ctx);

  // mode to track currentMode - made for education module to switch between q's
  Mode currentMode = Mode::Controller; //starts at home/controller

  auto renderMessage = [&](const UiRequest &req) {
    // For now we only support Message requests.
    // Changed, testing
    modePage.titleLabel->setText(QString::fromStdString(req.message.title));
    modePage.bodyLabel->setText(QString::fromStdString(req.message.body));

    if (currentMode == Mode::Educate) {
      modePage.nextBtn->setVisible(true);
      modePage.nextBtn->setText(QString::fromStdString(req.message.primaryButtonText));
    } else {
      modePage.nextBtn->setVisible(false);
    }
  };

  auto renderQuiz = [&](const UiRequest &req) {
    quizPage.questionLabel->setText(QString::fromStdString(req.quiz.question));

    for (int i = 0; i < 4; i++) {
      if (i < req.quiz.choices.size()) {
        quizPage.choiceButtons[i]->setText(
            QString::fromStdString(req.quiz.choices[i]));
        quizPage.choiceButtons[i]->setVisible(true);
      } else {
        quizPage.choiceButtons[i]->setVisible(false);
      }
    }

    stack->setCurrentWidget(quizPage.page);
  };

  //UI Renderer - works for Message, Quiz, Navigation

  std::function<void(const UiRequest&)> renderUiRequest = [&](const UiRequest& req) {
    if (req.kind == UiKind::Message) {
      // lesson page
      renderMessage(req);
      stack->setCurrentWidget(modePage.page);

    } else if (req.kind == UiKind::Quiz) {
      //show the quiz
      renderQuiz(req);

    } else if (req.kind == UiKind::Navigate) {
      //switch mode
      currentMode = req.nav.nextMode;
      UiRequest nextReq = runMode(req.nav.nextMode, ctx);
      //renders new mode
      renderUiRequest(nextReq);
    }
  };

  auto handleChoiceClick = [&](int choiceIndex) {
    UserInput input;
    input.kind = InputKind::ChoiceSelected;
    input.choiceIndex = choiceIndex;

    UiRequest req = educate_handle_input(input);
    //send to educate module
    renderUiRequest(req);
  };


  // SEPARATE LOOP
  for (int i = 0; i < 4; i++) {
    QObject::connect(quizPage.choiceButtons[i], &QPushButton::clicked, [&, i]() {
      handleChoiceClick(i);
    });
  }


  auto connectModeButton = [&](QPushButton *btn, Mode m) {
    QObject::connect(btn, &QPushButton::clicked, [&, m]() {
      currentMode = m; //remember which mode we're in

      UiRequest req = runMode(m, ctx);

      renderUiRequest(req);

    });
  };

  connectModeButton(home.trojanBtn, Mode::Trojan);
  connectModeButton(home.encryptBtn, Mode::Encrypt);
  connectModeButton(home.educateBtn, Mode::Educate);
  connectModeButton(home.restoreBtn, Mode::Restore);
  connectModeButton(home.errorBtn, Mode::Error);



  //Replaced below code to handle EXIT in Lession and Quiz pages
  auto handleExit = [&]() {
    currentMode = Mode::Controller;
    stack->setCurrentWidget(home.page);
  };

  QObject::connect(modePage.backBtn, &QPushButton::clicked, handleExit);
  QObject::connect(quizPage.backBtn, &QPushButton::clicked, handleExit);

  QObject::connect(modePage.nextBtn, &QPushButton::clicked, [&]() {
    if (currentMode == Mode::Educate) {
      UserInput input;
      input.kind = InputKind::PrimaryButton;

      UiRequest req = educate_handle_input(input);
      renderUiRequest(req);
    }
  });

  // QObject::connect(modePage.backBtn, &QPushButton::clicked, [&]() {
  //   // CHANGED to support mode navigation

  //   if (currentMode == Mode::Educate) {
  //     UserInput input;
  //     input.kind = InputKind::PrimaryButton;

  //     UiRequest req = educate_handle_input(input);
  //     renderUiRequest(req);

  //   } else if (currentMode == Mode::Trojan) {
  //     //TODO handle trojan - current : just go back to home
  //     currentMode = Mode::Controller;
  //     stack->setCurrentWidget(home.page);
  //   } else {
  //     //other modes - go back to home for now (MAYBE TODO)
  //     currentMode = Mode::Controller;
  //     stack->setCurrentWidget(home.page);
  //   }



  //   //stack->setCurrentWidget(home.page); // back to Home page OLD
  // });

  // TODO for testing
  // stack->setCurrentWidget(quizPage.page);

  window.show();
  return app.exec();
}