#pragma once

#include <QString>
#include <QWidget>

#include <functional>

class QLabel;
class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class PaintedLabel;

class StatusChoiceCard final : public QWidget
{
public:
    StatusChoiceCard(const QString& title, const QString& iconPath, int index, QWidget* parent = nullptr);

    int choiceIndex() const;
    void setSelected(bool selected);

    std::function<void(int)> clicked;

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void refresh();

    int m_choiceIndex = 0;
    QLabel* m_icon = nullptr;
    PaintedLabel* m_title = nullptr;
    QString m_iconPath;
    bool m_selected = false;
    bool m_hovered = false;
};
