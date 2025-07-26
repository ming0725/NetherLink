/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_POST_POST_CREATE_PAGE
#define INCLUDE_VIEW_POST_POST_CREATE_PAGE

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QPushButton>
#include <QTextEdit>

#include "Components/LineEditComponent.h"

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class PostCreatePage : public QWidget {
    Q_OBJECT

    public:
        explicit PostCreatePage(QWidget* parent = nullptr);

    signals:
        void postCreated();

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private slots:
        void onImageButtonClicked();

        void onSendButtonClicked();

        void handleImageSelected(const QString& path);

    private:
        void setupUI();

        void sendPost();

        QString mimeTypeForFile(const QString& filePath);

        LineEditComponent* m_titleEdit;
        QTextEdit* m_contentEdit;
        QPushButton* m_imageButton;
        QPushButton* m_sendButton;
        QLabel* m_imagePreview;
        QString m_selectedImagePath;
        static const int CORNER_RADIUS = 20;
        static const int MARGIN = 20;
        static const int SPACING = 15;
};

#endif /* INCLUDE_VIEW_POST_POST_CREATE_PAGE */
