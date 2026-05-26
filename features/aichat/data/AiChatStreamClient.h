#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

class AiChatStreamClient : public QObject
{
    Q_OBJECT

public:
    explicit AiChatStreamClient(QObject* parent = nullptr);

    bool isRunning() const;
    void start(const QString& prompt);
    void cancel();

signals:
    void chunkReceived(const QString& chunk);
    void finished();

private:
    struct StreamSegment {
        enum class Kind {
            Text,
            SettingCall
        };

        Kind kind = Kind::Text;
        QString text;
        QString action;
        QString value;
        QString label;
    };

    void emitNextChunk();
    QVector<StreamSegment> responseForPrompt(const QString& prompt) const;
    static StreamSegment textSegment(const QString& text);
    static StreamSegment settingSegment(const QString& action,
                                        const QString& value,
                                        const QString& label);
    static QString applySettingCall(const StreamSegment& segment);
    void scheduleNextChunk(int minDelayMs = 35, int maxDelayMs = 120);

    QTimer* m_timer = nullptr;
    QVector<StreamSegment> m_segments;
    int m_segmentIndex = 0;
    int m_offset = 0;
    bool m_running = false;
};
