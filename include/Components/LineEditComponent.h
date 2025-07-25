/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_COMPONENTS_LINE_EDIT_COMPONENT
#define INCLUDE_COMPONENTS_LINE_EDIT_COMPONENT

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QLabel>
#include <QLineEdit>
#include <QPainter>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class LineEditComponent : public QWidget {
    Q_OBJECT

    public:
        LineEditComponent(QWidget* parent = nullptr);

        ~LineEditComponent();

        QString currentText() const;

        void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;

        QLineEdit* getLineEdit() const;

        void setIcon(QPixmap);

        void setIconSize(QSize);

    protected:
        void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;

        bool eventFilter(QObject*watched, QEvent*event) Q_DECL_OVERRIDE;

        void resizeEvent(QResizeEvent*) Q_DECL_OVERRIDE;

    private:
        QLineEdit* lineEdit = Q_NULLPTR;
        QLabel* iconLabel = Q_NULLPTR;
        QToolButton* clearBtn = Q_NULLPTR;
        QPixmap iconPixmap;
        bool hasFocus = false;

    signals:
        void userAccountChanged(const QString& userAccount);

        void requestNextFocus();

        void lineEditUnfocused();

        void lineEditFocused();

};

#endif /* INCLUDE_COMPONENTS_LINE_EDIT_COMPONENT */
