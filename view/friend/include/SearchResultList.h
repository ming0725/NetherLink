
/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#pragma once

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include "../../components/include/CustomScrollArea.h"

#include "SearchResultItem.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class SearchFriendWindow;

class SearchResultList : public CustomScrollArea {
    Q_OBJECT

    public:
        explicit SearchResultList(QWidget* parent = nullptr);

        enum class SearchType {
            All,
            Users,
            Groups
        };

        void setSearchText(const QString& text);

        void setWindow(SearchFriendWindow* window) {
            searchWindow = window;
        }

        void setSearchType(SearchType type);

    protected:
        void layoutContent() override;

    private slots:
        void onSearchTimeout();

        void onMoreUsersClicked();

        void onMoreGroupsClicked();

        void onNetworkReplyFinished();

    private:
        void clearResults();

        void updateLayout();

        void searchUsers(const QString& keyword);

        void searchGroups(const QString& keyword);

        void processUserResults(const QJsonArray& users);

        void processGroupResults(const QJsonArray& groups);

        void handleNetworkError(QNetworkReply* reply);

        QList <SearchResultItem*> userItems;
        QList <SearchResultItem*> groupItems;
        QLabel* userSeparator;
        QLabel* userTitle;
        QLabel* userMore;
        QLabel* groupSeparator;
        QLabel* groupTitle;
        QLabel* groupMore;
        QString currentSearchText;
        QTimer* searchTimer;
        bool showAllUsers = false;
        bool showAllGroups = false;
        QNetworkAccessManager* networkManager;
        SearchFriendWindow* searchWindow = nullptr;
        SearchType currentSearchType = SearchType::All;

        // 布局常量
        const int MARGIN = 20;
        const int SEPARATOR_HEIGHT = 1;
        const int TITLE_HEIGHT = 30;
        const int MAX_ITEMS_SHOW = 5;
};
