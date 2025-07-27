/****************************************************************************
** Meta object code from reading C++ file 'PostPreviewItem.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/View/Post/PostPreviewItem.h"
#include <QtGui/qtextcursor.h>
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PostPreviewItem.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSPostPreviewItemENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSPostPreviewItemENDCLASS = QtMocHelpers::stringData(
    "PostPreviewItem",
    "viewPost",
    "",
    "postID",
    "viewAuthor",
    "loadFinished",
    "viewPostWithGeometry",
    "globalGeometry",
    "originalImage",
    "onViewPost",
    "onViewAuthor",
    "onClickLike"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSPostPreviewItemENDCLASS_t {
    uint offsetsAndSizes[24];
    char stringdata0[16];
    char stringdata1[9];
    char stringdata2[1];
    char stringdata3[7];
    char stringdata4[11];
    char stringdata5[13];
    char stringdata6[21];
    char stringdata7[15];
    char stringdata8[14];
    char stringdata9[11];
    char stringdata10[13];
    char stringdata11[12];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSPostPreviewItemENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSPostPreviewItemENDCLASS_t qt_meta_stringdata_CLASSPostPreviewItemENDCLASS = {
    {
        QT_MOC_LITERAL(0, 15),  // "PostPreviewItem"
        QT_MOC_LITERAL(16, 8),  // "viewPost"
        QT_MOC_LITERAL(25, 0),  // ""
        QT_MOC_LITERAL(26, 6),  // "postID"
        QT_MOC_LITERAL(33, 10),  // "viewAuthor"
        QT_MOC_LITERAL(44, 12),  // "loadFinished"
        QT_MOC_LITERAL(57, 20),  // "viewPostWithGeometry"
        QT_MOC_LITERAL(78, 14),  // "globalGeometry"
        QT_MOC_LITERAL(93, 13),  // "originalImage"
        QT_MOC_LITERAL(107, 10),  // "onViewPost"
        QT_MOC_LITERAL(118, 12),  // "onViewAuthor"
        QT_MOC_LITERAL(131, 11)   // "onClickLike"
    },
    "PostPreviewItem",
    "viewPost",
    "",
    "postID",
    "viewAuthor",
    "loadFinished",
    "viewPostWithGeometry",
    "globalGeometry",
    "originalImage",
    "onViewPost",
    "onViewAuthor",
    "onClickLike"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSPostPreviewItemENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   56,    2, 0x06,    1 /* Public */,
       4,    0,   59,    2, 0x06,    3 /* Public */,
       5,    0,   60,    2, 0x06,    4 /* Public */,
       6,    3,   61,    2, 0x06,    5 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       9,    0,   68,    2, 0x08,    9 /* Private */,
      10,    0,   69,    2, 0x08,   10 /* Private */,
      11,    0,   70,    2, 0x08,   11 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QRect, QMetaType::QPixmap,    3,    7,    8,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject PostPreviewItem::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_CLASSPostPreviewItemENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSPostPreviewItemENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSPostPreviewItemENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<PostPreviewItem, std::true_type>,
        // method 'viewPost'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'viewAuthor'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'viewPostWithGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<QRect, std::false_type>,
        QtPrivate::TypeAndForceComplete<QPixmap, std::false_type>,
        // method 'onViewPost'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onViewAuthor'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onClickLike'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void PostPreviewItem::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PostPreviewItem *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->viewPost((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->viewAuthor(); break;
        case 2: _t->loadFinished(); break;
        case 3: _t->viewPostWithGeometry((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QRect>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[3]))); break;
        case 4: _t->onViewPost(); break;
        case 5: _t->onViewAuthor(); break;
        case 6: _t->onClickLike(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (PostPreviewItem::*)(QString );
            if (_t _q_method = &PostPreviewItem::viewPost; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (PostPreviewItem::*)();
            if (_t _q_method = &PostPreviewItem::viewAuthor; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (PostPreviewItem::*)();
            if (_t _q_method = &PostPreviewItem::loadFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (PostPreviewItem::*)(QString , QRect , QPixmap );
            if (_t _q_method = &PostPreviewItem::viewPostWithGeometry; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
}

const QMetaObject *PostPreviewItem::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PostPreviewItem::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSPostPreviewItemENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int PostPreviewItem::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void PostPreviewItem::viewPost(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void PostPreviewItem::viewAuthor()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void PostPreviewItem::loadFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void PostPreviewItem::viewPostWithGeometry(QString _t1, QRect _t2, QPixmap _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
