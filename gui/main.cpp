#include <QApplication>
#include <QPalette>

#include "mainwindow.h"

static const char *darkStyleSheet = R"(
    /* Global dark theme */
    QWidget {
        background-color: #111827;
        color: #e5e7eb;
        font-family: "Segoe UI", "Inter", "Noto Sans", "Liberation Sans", sans-serif;
    }

    QMainWindow {
        background-color: #111827;
    }

    /* Tooltips */
    QToolTip {
        background-color: #1f2937;
        color: #e5e7eb;
        border: 1px solid #374151;
        border-radius: 4px;
        padding: 4px 8px;
        font-size: 11px;
    }

    /* Scrollbars */
    QScrollBar:vertical {
        background: #111827;
        width: 8px;
        margin: 0;
        border-radius: 4px;
    }
    QScrollBar::handle:vertical {
        background: #374151;
        min-height: 30px;
        border-radius: 4px;
    }
    QScrollBar::handle:vertical:hover {
        background: #4b5563;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
    }
    QScrollBar:horizontal {
        background: #111827;
        height: 8px;
        margin: 0;
        border-radius: 4px;
    }
    QScrollBar::handle:horizontal {
        background: #374151;
        min-width: 30px;
        border-radius: 4px;
    }
    QScrollBar::handle:horizontal:hover {
        background: #4b5563;
    }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
        width: 0px;
    }

    /* Sliders */
    QSlider::groove:horizontal {
        border: none;
        height: 4px;
        background: #374151;
        border-radius: 2px;
    }
    QSlider::handle:horizontal {
        background: #76b900;
        border: 2px solid #5a8f00;
        width: 14px;
        height: 14px;
        margin: -6px 0;
        border-radius: 8px;
    }
    QSlider::handle:horizontal:hover {
        background: #8cd400;
        border: 2px solid #76b900;
    }
    QSlider::handle:horizontal:disabled {
        background: #4b5563;
        border: 2px solid #374151;
    }
    QSlider::sub-page:horizontal {
        background: #76b900;
        border-radius: 2px;
    }
    QSlider::sub-page:horizontal:disabled {
        background: #374151;
    }

    /* Menu (for system tray) */
    QMenu {
        background-color: #1f2937;
        color: #e5e7eb;
        border: 1px solid #374151;
        border-radius: 6px;
        padding: 4px;
    }
    QMenu::item {
        padding: 6px 24px;
        border-radius: 4px;
    }
    QMenu::item:selected {
        background-color: #374151;
    }
    QMenu::separator {
        height: 1px;
        background: #374151;
        margin: 4px 8px;
    }

    /* Message boxes */
    QMessageBox {
        background-color: #1f2937;
    }
    QMessageBox QLabel {
        color: #e5e7eb;
    }
    QPushButton {
        background-color: #374151;
        color: #e5e7eb;
        border: 1px solid #4b5563;
        border-radius: 6px;
        padding: 6px 16px;
    }
    QPushButton:hover {
        background-color: #4b5563;
    }
    QPushButton:pressed {
        background-color: #1f2937;
    }
)";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Maxine PipeWire"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("maxine-pipewire"));
    app.setDesktopFileName(QStringLiteral("maxine-gui"));

    // Apply dark theme
    app.setStyleSheet(QString::fromLatin1(darkStyleSheet));

    // Set dark palette as fallback
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(0x11, 0x18, 0x27));
    darkPalette.setColor(QPalette::WindowText, QColor(0xe5, 0xe7, 0xeb));
    darkPalette.setColor(QPalette::Base, QColor(0x1f, 0x29, 0x37));
    darkPalette.setColor(QPalette::AlternateBase, QColor(0x37, 0x41, 0x51));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(0x1f, 0x29, 0x37));
    darkPalette.setColor(QPalette::ToolTipText, QColor(0xe5, 0xe7, 0xeb));
    darkPalette.setColor(QPalette::Text, QColor(0xe5, 0xe7, 0xeb));
    darkPalette.setColor(QPalette::Button, QColor(0x37, 0x41, 0x51));
    darkPalette.setColor(QPalette::ButtonText, QColor(0xe5, 0xe7, 0xeb));
    darkPalette.setColor(QPalette::BrightText, QColor(0x76, 0xb9, 0x00));
    darkPalette.setColor(QPalette::Link, QColor(0x76, 0xb9, 0x00));
    darkPalette.setColor(QPalette::Highlight, QColor(0x76, 0xb9, 0x00));
    darkPalette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6b, 0x72, 0x80));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6b, 0x72, 0x80));
    app.setPalette(darkPalette);

    MainWindow window;
    window.show();

    return app.exec();
}
