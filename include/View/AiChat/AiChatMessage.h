/* guard ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */

#ifndef INCLUDE_VIEW_AI_CHAT_AI_CHAT_MESSAGE

#define INCLUDE_VIEW_AI_CHAT_AI_CHAT_MESSAGE

/* include ---------------------------------------------------------------- 80 // ! ----------------------------- 120 */

#include <QDateTime>

/* class ------------------------------------------------------------------ 80 // ! ----------------------------- 120 */
class AiChatMessage {
    Q_GADGET

    public:
        enum Role {
            User,
            AI
        };

        Q_ENUM(Role) AiChatMessage(Role role, const QString& content, const QString& messageId = QString()) : m_role(role), m_content(content), m_messageId(messageId), m_time(QDateTime::currentDateTime()), m_isSelected(false) {}

        Role role() const {
            return (m_role);
        }

        QString content() const {
            return (m_content);
        }

        QString messageId() const {
            return (m_messageId);
        }

        QDateTime time() const {
            return (m_time);
        }

        bool isSelected() const {
            return (m_isSelected);
        }

        void setContent(const QString& content) {
            m_content = content;
        }

        void setSelected(bool selected) {
            m_isSelected = selected;
        }

        void appendContent(const QString& content) {
            m_content += content;
        }

    private:
        Role m_role;
        QString m_content;
        QString m_messageId;
        QDateTime m_time;
        bool m_isSelected;
};
Q_DECLARE_METATYPE(AiChatMessage*)

#endif /* INCLUDE_VIEW_AI_CHAT_AI_CHAT_MESSAGE */
