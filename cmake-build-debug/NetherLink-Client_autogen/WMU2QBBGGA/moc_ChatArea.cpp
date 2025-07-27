/****************************************************************************
** Meta object code from reading C++ file 'ChatArea.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/View/Chat/ChatArea.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ChatArea.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSChatAreaENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSChatAreaENDCLASS = QtMocHelpers::stringData(
    "ChatArea",
    "onScrollValueChanged",
    "",
    "value",
    "onNewMessageNotifierClicked",
    "onSendImage",
    "path",
    "onSendText",
    "text",
    "onMessageReceived",
    "ChatMessagePtr",
    "message"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSChatAreaENDCLASS_t {
    uint offsetsAndSizes[24];
    char stringdata0[9];
    char stringdata1[21];
    char stringdata2[1];
    char stringdata3[6];
    char stringdata4[28];
    char stringdata5[12];
    char stringdata6[5];
    char stringdata7[11];
    char stringdata8[5];
    char stringdata9[18];
    char stringdata10[15];
    char stringdata11[8];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSChatAreaENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSChatAreaENDCLASS_t qt_meta_stringdata_CLASSChatAreaENDCLASS = {
    {
        QT_MOC_LITERAL(0, 8),  // "ChatArea"
        QT_MOC_LITERAL(9, 20),  // "onScrollValueChanged"
        QT_MOC_LITERAL(30, 0),  // ""
        QT_MOC_LITERAL(31, 5),  // "value"
        QT_MOC_LITERAL(37, 27),  // "onNewMessageNotifierClicked"
        QT_MOC_LITERAL(65, 11),  // "onSendImage"
        QT_MOC_LITERAL(77, 4),  // "path"
        QT_MOC_LITERAL(82, 10),  // "onSendText"
        QT_MOC_LITERAL(93, 4),  // "text"
        QT_MOC_LITERAL(98, 17),  // "onMessageReceived"
        QT_MOC_LITERAL(116, 14),  // "ChatMessagePtr"
        QT_MOC_LITERAL(131, 7)   // "message"
    },
    "ChatArea",
    "onScrollValueChanged",
    "",
    "value",
    "onNewMessageNotifierClicked",
    "onSendImage",
    "path",
    "onSendText",
    "text",
    "onMessageReceived",
    "ChatMessagePtr",
    "message"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSChatAreaENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   44,    2, 0x08,    1 /* Private */,
       4,    0,   47,    2, 0x08,    3 /* Private */,
       5,    1,   48,    2, 0x08,    4 /* Private */,
       7,    1,   51,    2, 0x08,    6 /* Private */,
       9,    1,   54,    2, 0x08,    8 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    6,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void, 0x80000000 | 10,   11,

       0        // eod
};

Q_CONSTINIT const QMetaObject ChatArea::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_CLASSChatAreaENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSChatAreaENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSChatAreaENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ChatArea, std::true_type>,
        // method 'onScrollValueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onNewMessageNotifierClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSendImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onSendText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onMessageReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<ChatMessagePtr, std::false_type>
    >,
    nullptr
} };

void ChatArea::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ChatArea *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onScrollValueChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->onNewMessageNotifierClicked(); break;
        case 2: _t->onSendImage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->onSendText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->onMessageReceived((*reinterpret_cast< std::add_pointer_t<ChatMessagePtr>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *ChatArea::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ChatArea::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSChatAreaENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ChatArea::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}
QT_WARNING_POP
