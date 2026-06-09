#include <QApplication>
#include <QTimer>
#include "app/frame/login/LoginWindow.h"
#include "app/frame/login/LoginAccountRepository.h"
#include "app/frame/MainWindow.h"
#include "app/state/CurrentUser.h"
#include "app/state/CurrentUserRemoteDataSource.h"
#include "shared/network/AuthSession.h"
#include "shared/network/HttpClient.h"
#include "shared/network/NetworkLog.h"
#include "shared/network/NetworkService.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"

#include <QPointer>

#include <functional>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NetworkLog::installApplicationMessageHandler();
    a.setQuitOnLastWindowClosed(true);
    ThemeManager::instance().applyToApplication(a);
    AudioService::instance().preloadUiSounds();

    QPointer<LoginWindow> loginWindow;
    QPointer<MainWindow> mainWindow;
    QString activeLoginAccountId;

    auto clearActiveLoginMarker = [&]() {
        const QString accountId = activeLoginAccountId.isEmpty()
                ? AuthSession::instance().loginAccountId()
                : activeLoginAccountId;
        if (!accountId.isEmpty()) {
            LoginAccountRepository::instance().setAccountLoggedInOnDevice(accountId, false);
        }
        activeLoginAccountId.clear();
    };

    std::function<void(bool)> showLoginWindow;
    std::function<void()> showMainWindow;

    showLoginWindow = [&](bool suppressAutoLogin) {
        auto* window = new LoginWindow;
        if (suppressAutoLogin) {
            window->suppressNextAutoLogin();
        }
        loginWindow = window;

        QObject::connect(window, &LoginWindow::loginAccepted, window, [&](const QString& accountId) {
            LoginWindow* acceptedLoginWindow = loginWindow;
            loginWindow = nullptr;
            activeLoginAccountId = accountId.trimmed();
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

        auto completeLogout = [&](bool suppressAutoLogin) {
            clearActiveLoginMarker();
            MainWindow* loggedOutMainWindow = mainWindow;
            mainWindow = nullptr;
            if (!loginWindow) {
                showLoginWindow(suppressAutoLogin);
            }
            if (loggedOutMainWindow) {
                loggedOutMainWindow->hide();
                loggedOutMainWindow->deleteLater();
            }
        };

        QObject::connect(window, &MainWindow::logoutRequested, window, [&, completeLogout]() {
            const QString loggedOutAccountId = activeLoginAccountId.isEmpty()
                    ? AuthSession::instance().loginAccountId()
                    : activeLoginAccountId;
            if (!AuthSession::instance().hasAccessToken() && !AuthSession::instance().hasRefreshToken()) {
                NetworkService::instance().stopRealtime();
                AuthSession::instance().clear();
                CurrentUser::instance().clear();
                if (!loggedOutAccountId.isEmpty()) {
                    LoginAccountRepository::instance().clearAccountTokens(loggedOutAccountId);
                }
                completeLogout(true);
                return;
            }

            NetworkService::instance().logout();
            CurrentUser::instance().clear();
            if (!loggedOutAccountId.isEmpty()) {
                LoginAccountRepository::instance().clearAccountTokens(loggedOutAccountId);
            }
            completeLogout(true);
        });
        QObject::connect(&NetworkService::instance(),
                         &NetworkService::sessionExpired,
                         window,
                         [&, window, completeLogout](const NetworkError&) {
            if (mainWindow != window) {
                return;
            }
            const QString expiredAccountId = activeLoginAccountId.isEmpty()
                    ? AuthSession::instance().loginAccountId()
                    : activeLoginAccountId;
            if (!expiredAccountId.isEmpty()) {
                LoginAccountRepository::instance().clearAccountTokens(expiredAccountId);
            }
            CurrentUser::instance().clear();
            GlobalNotification::showFailure(window, QStringLiteral("登录状态已过期"));
            QTimer::singleShot(900, window, [&, window, completeLogout]() {
                if (mainWindow == window) {
                    completeLogout(true);
                }
            });
        });
        QObject::connect(window, &QObject::destroyed, &a, [&, window]() {
            if (mainWindow == window) {
                clearActiveLoginMarker();
            }
            if (mainWindow == window) {
                mainWindow = nullptr;
            }
        });

        window->show();
        window->raise();
        window->activateWindow();
    };

    QObject::connect(&a, &QCoreApplication::aboutToQuit, &a, clearActiveLoginMarker);

    QObject::connect(&HttpClient::instance(),
                     &HttpClient::authRefreshSucceeded,
                     &a,
                     [](const QString& accessToken, const QString& refreshToken, int expiresInSeconds) {
        const QString accountId = AuthSession::instance().loginAccountId();
        if (!accountId.isEmpty()) {
            LoginAccountRepository::instance().saveAccountTokens(accountId,
                                                                 accessToken,
                                                                 refreshToken,
                                                                 expiresInSeconds);
            CurrentUserRemoteDataSource::instance().fetchPreferences();
        }
    });

    showLoginWindow(false);
    return a.exec();
}
