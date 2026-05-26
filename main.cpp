#include <QApplication>
#include <QTimer>
#include "app/frame/login/LoginWindow.h"
#include "app/frame/MainWindow.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"

#include <QPointer>

#include <functional>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(true);
    ThemeManager::instance().applyToApplication(a);
    AudioService::instance().preloadUiSounds();

    QPointer<LoginWindow> loginWindow;
    QPointer<MainWindow> mainWindow;

    std::function<void()> showLoginWindow;
    std::function<void()> showMainWindow;

    showLoginWindow = [&]() {
        auto* window = new LoginWindow;
        loginWindow = window;

        QObject::connect(window, &LoginWindow::loginAccepted, window, [&]() {
            LoginWindow* acceptedLoginWindow = loginWindow;
            loginWindow = nullptr;
            if (acceptedLoginWindow) {
                acceptedLoginWindow->hide();
                acceptedLoginWindow->deleteLater();
            }
            QTimer::singleShot(0, &a, [&]() {
                if (!mainWindow && !loginWindow) {
                    showMainWindow();
                }
            });
        });
        QObject::connect(window, &QObject::destroyed, &a, [&, window]() {
            if (loginWindow == window) {
                loginWindow = nullptr;
            }
        });

        window->show();
    };

    showMainWindow = [&]() {
        auto* window = new MainWindow;
        mainWindow = window;

        QObject::connect(window, &MainWindow::logoutRequested, window, [&]() {
            MainWindow* loggedOutMainWindow = mainWindow;
            mainWindow = nullptr;
            if (loggedOutMainWindow) {
                loggedOutMainWindow->hide();
                loggedOutMainWindow->deleteLater();
            }
            QTimer::singleShot(0, &a, [&]() {
                if (!loginWindow && !mainWindow) {
                    showLoginWindow();
                }
            });
        });
        QObject::connect(window, &QObject::destroyed, &a, [&, window]() {
            if (mainWindow == window) {
                mainWindow = nullptr;
            }
        });

        window->show();
        window->raise();
        window->activateWindow();
    };

    showLoginWindow();
    return a.exec();
}
