#pragma once

#include <QLineEdit>
#include <QWidget>

class LoginInputField : public QWidget
{
    Q_OBJECT

public:
    explicit LoginInputField(QWidget* parent = nullptr);

    void setDropdownEnabled(bool enabled);
    bool dropdownEnabled() const;

    void setPasswordMode(bool enabled);
    void setDigitsOnly(bool enabled);
    void setPlaceholderText(const QString& text);
    void setText(const QString& text);
    QString text() const;

    QLineEdit* lineEdit() const;

signals:
    void dropdownRequested();
    void textChanged(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QRect clearButtonRect() const;
    QRect dropdownButtonRect() const;
    void updateLineEditGeometry();
    void updateTextMargins();
    void updateClearButtonState();

    QLineEdit* m_lineEdit = nullptr;
    bool m_dropdownEnabled = false;
    bool m_hovered = false;
    bool m_hasEditFocus = false;
};
