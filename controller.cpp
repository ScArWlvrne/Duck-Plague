#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;                       // A blank window
    window.setWindowTitle("Duck Plague"); // Window title bar text

    auto *layout = new QVBoxLayout(&window); // Vertical layout inside the window

    auto *label = new QLabel("Controller: Home Screen");
    auto *button = new QPushButton("Enter Trojan Mode");

    layout->addWidget(label);
    layout->addWidget(button);

    QObject::connect(button, &QPushButton::clicked, [&]() {
        label->setText("Entered: Trojan Mode");
    });

    window.show();
    return app.exec();
}
