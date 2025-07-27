/****************************************************************************
** Meta object code from reading C++ file 'AiChatWebSocket.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/View/AiChat/AiChatWebSocket.h"
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AiChatWebSocket.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.5.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS = QtMocHelpers::stringData(
    "AiChatWebSocket",
    "messageContent",
    "",
    "content",
    "conversationStarted",
    "conversationId",
    "messageEnded",
    "connectionEstablished",
    "connectionError",
    "error",
    "requestError",
    "errorMessage",
    "onConnected",
    "onDisconnected",
    "onTextMessageReceived",
    "message",
    "onError",
    "QAbstractSocket::SocketError"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS_t {
    uint offsetsAndSizes[36];
    char stringdata0[16];
    char stringdata1[15];
    char stringdata2[1];
    char stringdata3[8];
    char stringdata4[20];
    char stringdata5[15];
    char stringdata6[13];
    char stringdata7[22];
    char stringdata8[16];
    char stringdata9[6];
    char stringdata10[13];
    char stringdata11[13];
    char stringdata12[12];
    char stringdata13[15];
    char stringdata14[22];
    char stringdata15[8];
    char stringdata16[8];
    char stringdata17[29];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS_t qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS = {
    {
        QT_MOC_LITERAL(0, 15),  // "AiChatWebSocket"
        QT_MOC_LITERAL(16, 14),  // "messageContent"
        QT_MOC_LITERAL(31, 0),  // ""
        QT_MOC_LITERAL(32, 7),  // "content"
        QT_MOC_LITERAL(40, 19),  // "conversationStarted"
        QT_MOC_LITERAL(60, 14),  // "conversationId"
        QT_MOC_LITERAL(75, 12),  // "messageEnded"
        QT_MOC_LITERAL(88, 21),  // "connectionEstablished"
        QT_MOC_LITERAL(110, 15),  // "connectionError"
        QT_MOC_LITERAL(126, 5),  // "error"
        QT_MOC_LITERAL(132, 12),  // "requestError"
        QT_MOC_LITERAL(145, 12),  // "errorMessage"
        QT_MOC_LITERAL(158, 11),  // "onConnected"
        QT_MOC_LITERAL(170, 14),  // "onDisconnected"
        QT_MOC_LITERAL(185, 21),  // "onTextMessageReceived"
        QT_MOC_LITERAL(207, 7),  // "message"
        QT_MOC_LITERAL(215, 7),  // "onError"
        QT_MOC_LITERAL(223, 28)   // "QAbstractSocket::SocketError"
    },
    "AiChatWebSocket",
    "messageContent",
    "",
    "content",
    "conversationStarted",
    "conversationId",
    "messageEnded",
    "connectionEstablished",
    "connectionError",
    "error",
    "requestError",
    "errorMessage",
    "onConnected",
    "onDisconnected",
    "onTextMessageReceived",
    "message",
    "onError",
    "QAbstractSocket::SocketError"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSAiChatWebSocketENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   74,    2, 0x06,    1 /* Public */,
       4,    1,   77,    2, 0x06,    3 /* Public */,
       6,    0,   80,    2, 0x06,    5 /* Public */,
       7,    0,   81,    2, 0x06,    6 /* Public */,
       8,    1,   82,    2, 0x06,    7 /* Public */,
      10,    1,   85,    2, 0x06,    9 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      12,    0,   88,    2, 0x08,   11 /* Private */,
      13,    0,   89,    2, 0x08,   12 /* Private */,
      14,    1,   90,    2, 0x08,   13 /* Private */,
      16,    1,   93,    2, 0x08,   15 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    9,
    QMetaType::Void, QMetaType::QString,   11,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void, 0x80000000 | 17,    9,

       0        // eod
};

Q_CONSTINIT const QMetaObject AiChatWebSocket::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSAiChatWebSocketENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AiChatWebSocket, std::true_type>,
        // method 'messageContent'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'conversationStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'messageEnded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'connectionEstablished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'connectionError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'requestError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onConnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDisconnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onTextMessageReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QAbstractSocket::SocketError, std::false_type>
    >,
    nullptr
} };

void AiChatWebSocket::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AiChatWebSocket *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->messageContent((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->conversationStarted((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->messageEnded(); break;
        case 3: _t->connectionEstablished(); break;
        case 4: _t->connectionError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->requestError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->onConnected(); break;
        case 7: _t->onDisconnected(); break;
        case 8: _t->onTextMessageReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->onError((*reinterpret_cast< std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AiChatWebSocket::*)(const QString & );
            if (_t _q_method = &AiChatWebSocket::messageContent; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AiChatWebSocket::*)(const QString & );
            if (_t _q_method = &AiChatWebSocket::conversationStarted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AiChatWebSocket::*)();
            if (_t _q_method = &AiChatWebSocket::messageEnded; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AiChatWebSocket::*)();
            if (_t _q_method = &AiChatWebSocket::connectionEstablished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (AiChatWebSocket::*)(const QString & );
            if (_t _q_method = &AiChatWebSocket::connectionError; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (AiChatWebSocket::*)(const QString & );
            if (_t _q_method = &AiChatWebSocket::requestError; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
    }
}

const QMetaObject *AiChatWebSocket::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AiChatWebSocket::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSAiChatWebSocketENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AiChatWebSocket::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void AiChatWebSocket::messageContent(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void AiChatWebSocket::conversationStarted(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void AiChatWebSocket::messageEnded()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void AiChatWebSocket::connectionEstablished()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void AiChatWebSocket::connectionError(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void AiChatWebSocket::requestError(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}
QT_WARNING_POP
