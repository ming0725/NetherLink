/****************************************************************************
** Meta object code from reading C++ file 'AiChatListWidget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/View/AiChat/AiChatListWidget.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AiChatListWidget.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS = QtMocHelpers::stringData(
    "AiChatListWidget",
    "chatItemClicked",
    "",
    "AiChatListItem*",
    "item",
    "chatItemRenamed",
    "chatItemDeleted",
    "chatOrderChanged",
    "onItemClicked"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS_t {
    uint offsetsAndSizes[18];
    char stringdata0[17];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[16];
    char stringdata4[5];
    char stringdata5[16];
    char stringdata6[16];
    char stringdata7[17];
    char stringdata8[14];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS_t qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS = {
    {
        QT_MOC_LITERAL(0, 16),  // "AiChatListWidget"
        QT_MOC_LITERAL(17, 15),  // "chatItemClicked"
        QT_MOC_LITERAL(33, 0),  // ""
        QT_MOC_LITERAL(34, 15),  // "AiChatListItem*"
        QT_MOC_LITERAL(50, 4),  // "item"
        QT_MOC_LITERAL(55, 15),  // "chatItemRenamed"
        QT_MOC_LITERAL(71, 15),  // "chatItemDeleted"
        QT_MOC_LITERAL(87, 16),  // "chatOrderChanged"
        QT_MOC_LITERAL(104, 13)   // "onItemClicked"
    },
    "AiChatListWidget",
    "chatItemClicked",
    "",
    "AiChatListItem*",
    "item",
    "chatItemRenamed",
    "chatItemDeleted",
    "chatOrderChanged",
    "onItemClicked"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSAiChatListWidgetENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   44,    2, 0x06,    1 /* Public */,
       5,    1,   47,    2, 0x06,    3 /* Public */,
       6,    1,   50,    2, 0x06,    5 /* Public */,
       7,    0,   53,    2, 0x06,    7 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       8,    1,   54,    2, 0x08,    8 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    2,

       0        // eod
};

Q_CONSTINIT const QMetaObject AiChatListWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<CustomScrollArea::staticMetaObject>(),
    qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSAiChatListWidgetENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AiChatListWidget, std::true_type>,
        // method 'chatItemClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<AiChatListItem *, std::false_type>,
        // method 'chatItemRenamed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<AiChatListItem *, std::false_type>,
        // method 'chatItemDeleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<AiChatListItem *, std::false_type>,
        // method 'chatOrderChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onItemClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<AiChatListItem *, std::false_type>
    >,
    nullptr
} };

void AiChatListWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AiChatListWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->chatItemClicked((*reinterpret_cast< std::add_pointer_t<AiChatListItem*>>(_a[1]))); break;
        case 1: _t->chatItemRenamed((*reinterpret_cast< std::add_pointer_t<AiChatListItem*>>(_a[1]))); break;
        case 2: _t->chatItemDeleted((*reinterpret_cast< std::add_pointer_t<AiChatListItem*>>(_a[1]))); break;
        case 3: _t->chatOrderChanged(); break;
        case 4: _t->onItemClicked((*reinterpret_cast< std::add_pointer_t<AiChatListItem*>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< AiChatListItem* >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< AiChatListItem* >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< AiChatListItem* >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< AiChatListItem* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AiChatListWidget::*)(AiChatListItem * );
            if (_t _q_method = &AiChatListWidget::chatItemClicked; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AiChatListWidget::*)(AiChatListItem * );
            if (_t _q_method = &AiChatListWidget::chatItemRenamed; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AiChatListWidget::*)(AiChatListItem * );
            if (_t _q_method = &AiChatListWidget::chatItemDeleted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AiChatListWidget::*)();
            if (_t _q_method = &AiChatListWidget::chatOrderChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
}

const QMetaObject *AiChatListWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AiChatListWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSAiChatListWidgetENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return CustomScrollArea::qt_metacast(_clname);
}

int AiChatListWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = CustomScrollArea::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void AiChatListWidget::chatItemClicked(AiChatListItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void AiChatListWidget::chatItemRenamed(AiChatListItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void AiChatListWidget::chatItemDeleted(AiChatListItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void AiChatListWidget::chatOrderChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
