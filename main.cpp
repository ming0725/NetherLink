#include <QApplication>
#include <QTimer>
#include "app/frame/login/LoginWindow.h"
#include "app/frame/MainWindow.h"
#include "app/state/CurrentUser.h"
#include "shared/network/AuthSession.h"
#include "shared/network/NetworkService.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"

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

        auto completeLogout = [&]() {
            MainWindow* loggedOutMainWindow = mainWindow;
            mainWindow = nullptr;
            if (!loginWindow) {
                showLoginWindow();
            }
            if (loggedOutMainWindow) {
                loggedOutMainWindow->hide();
                loggedOutMainWindow->deleteLater();
            }
        };

        QObject::connect(window, &MainWindow::logoutRequested, window, [&, completeLogout]() {
            if (!AuthSession::instance().hasAccessToken() && !AuthSession::instance().hasRefreshToken()) {
                NetworkService::instance().stopRealtime();
                AuthSession::instance().clear();
                CurrentUser::instance().clear();
                completeLogout();
                return;
            }

            NetworkService::instance().logout();
            CurrentUser::instance().clear();
            completeLogout();
        });
        QObject::connect(&NetworkService::instance(),
                         &NetworkService::sessionExpired,
                         window,
                         [&, window, completeLogout](const NetworkError&) {
            if (mainWindow != window) {
                return;
            }
            CurrentUser::instance().clear();
            GlobalNotification::showFailure(window, QStringLiteral("登录状态已过期"));
            QTimer::singleShot(900, window, [&, window, completeLogout]() {
                if (mainWindow == window) {
                    completeLogout();
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
