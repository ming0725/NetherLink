/****************************************************************************
** Meta object code from reading C++ file 'MessageRepository.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/Data/MessageRepository.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QSharedPointer>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MessageRepository.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSMessageRepositoryENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSMessageRepositoryENDCLASS = QtMocHelpers::stringData(
    "MessageRepository",
    "lastMessageChanged",
    "",
    "conversationId",
    "QSharedPointer<ChatMessage>",
    "lastMessage",
    "addMessage",
    "message",
    "removeMessage",
    "index",
    "isGroupChat"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSMessageRepositoryENDCLASS_t {
    uint offsetsAndSizes[22];
    char stringdata0[18];
    char stringdata1[19];
    char stringdata2[1];
    char stringdata3[15];
    char stringdata4[28];
    char stringdata5[12];
    char stringdata6[11];
    char stringdata7[8];
    char stringdata8[14];
    char stringdata9[6];
    char stringdata10[12];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSMessageRepositoryENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSMessageRepositoryENDCLASS_t qt_meta_stringdata_CLASSMessageRepositoryENDCLASS = {
    {
        QT_MOC_LITERAL(0, 17),  // "MessageRepository"
        QT_MOC_LITERAL(18, 18),  // "lastMessageChanged"
        QT_MOC_LITERAL(37, 0),  // ""
        QT_MOC_LITERAL(38, 14),  // "conversationId"
        QT_MOC_LITERAL(53, 27),  // "QSharedPointer<ChatMessage>"
        QT_MOC_LITERAL(81, 11),  // "lastMessage"
        QT_MOC_LITERAL(93, 10),  // "addMessage"
        QT_MOC_LITERAL(104, 7),  // "message"
        QT_MOC_LITERAL(112, 13),  // "removeMessage"
        QT_MOC_LITERAL(126, 5),  // "index"
        QT_MOC_LITERAL(132, 11)   // "isGroupChat"
    },
    "MessageRepository",
    "lastMessageChanged",
    "",
    "conversationId",
    "QSharedPointer<ChatMessage>",
    "lastMessage",
    "addMessage",
    "message",
    "removeMessage",
    "index",
    "isGroupChat"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSMessageRepositoryENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   32,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       6,    2,   37,    2, 0x0a,    4 /* Public */,
       8,    3,   42,    2, 0x0a,    7 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString, 0x80000000 | 4,    3,    5,

 // slots: parameters
    QMetaType::Void, QMetaType::QString, 0x80000000 | 4,    3,    7,
    QMetaType::Void, QMetaType::QString, QMetaType::Int, QMetaType::Bool,    3,    9,   10,

       0        // eod
};

Q_CONSTINIT const QMetaObject MessageRepository::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSMessageRepositoryENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSMessageRepositoryENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSMessageRepositoryENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MessageRepository, std::true_type>,
        // method 'lastMessageChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<QSharedPointer<ChatMessage>, std::false_type>,
        // method 'addMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<QSharedPointer<ChatMessage>, std::false_type>,
        // method 'removeMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void MessageRepository::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MessageRepository *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->lastMessageChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QSharedPointer<ChatMessage>>>(_a[2]))); break;
        case 1: _t->addMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QSharedPointer<ChatMessage>>>(_a[2]))); break;
        case 2: _t->removeMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MessageRepository::*)(const QString & , QSharedPointer<ChatMessage> );
            if (_t _q_method = &MessageRepository::lastMessageChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *MessageRepository::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MessageRepository::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSMessageRepositoryENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int MessageRepository::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void MessageRepository::lastMessageChanged(const QString & _t1, QSharedPointer<ChatMessage> _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
