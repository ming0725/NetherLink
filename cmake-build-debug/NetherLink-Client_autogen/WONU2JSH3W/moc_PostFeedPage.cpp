/****************************************************************************
** Meta object code from reading C++ file 'PostFeedPage.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/View/Post/PostFeedPage.h"
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
#error "The header file 'PostFeedPage.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSPostFeedPageENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSPostFeedPageENDCLASS = QtMocHelpers::stringData(
    "PostFeedPage",
    "loadMore",
    "",
    "postClicked",
    "postID",
    "postClickedWithGeometry",
    "globalGeometry",
    "originalImage"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSPostFeedPageENDCLASS_t {
    uint offsetsAndSizes[16];
    char stringdata0[13];
    char stringdata1[9];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[7];
    char stringdata5[24];
    char stringdata6[15];
    char stringdata7[14];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSPostFeedPageENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSPostFeedPageENDCLASS_t qt_meta_stringdata_CLASSPostFeedPageENDCLASS = {
    {
        QT_MOC_LITERAL(0, 12),  // "PostFeedPage"
        QT_MOC_LITERAL(13, 8),  // "loadMore"
        QT_MOC_LITERAL(22, 0),  // ""
        QT_MOC_LITERAL(23, 11),  // "postClicked"
        QT_MOC_LITERAL(35, 6),  // "postID"
        QT_MOC_LITERAL(42, 23),  // "postClickedWithGeometry"
        QT_MOC_LITERAL(66, 14),  // "globalGeometry"
        QT_MOC_LITERAL(81, 13)   // "originalImage"
    },
    "PostFeedPage",
    "loadMore",
    "",
    "postClicked",
    "postID",
    "postClickedWithGeometry",
    "globalGeometry",
    "originalImage"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSPostFeedPageENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   32,    2, 0x06,    1 /* Public */,
       3,    1,   33,    2, 0x06,    2 /* Public */,
       5,    3,   36,    2, 0x06,    4 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void, QMetaType::QString, QMetaType::QRect, QMetaType::QPixmap,    4,    6,    7,

       0        // eod
};

Q_CONSTINIT const QMetaObject PostFeedPage::staticMetaObject = { {
    QMetaObject::SuperData::link<CustomScrollArea::staticMetaObject>(),
    qt_meta_stringdata_CLASSPostFeedPageENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSPostFeedPageENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSPostFeedPageENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<PostFeedPage, std::true_type>,
        // method 'loadMore'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'postClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'postClickedWithGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<QRect, std::false_type>,
        QtPrivate::TypeAndForceComplete<QPixmap, std::false_type>
    >,
    nullptr
} };

void PostFeedPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PostFeedPage *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->loadMore(); break;
        case 1: _t->postClicked((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->postClickedWithGeometry((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QRect>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (PostFeedPage::*)();
            if (_t _q_method = &PostFeedPage::loadMore; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (PostFeedPage::*)(QString );
            if (_t _q_method = &PostFeedPage::postClicked; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (PostFeedPage::*)(QString , QRect , QPixmap );
            if (_t _q_method = &PostFeedPage::postClickedWithGeometry; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *PostFeedPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PostFeedPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSPostFeedPageENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return CustomScrollArea::qt_metacast(_clname);
}

int PostFeedPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = CustomScrollArea::qt_metacall(_c, _id, _a);
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
void PostFeedPage::loadMore()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void PostFeedPage::postClicked(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void PostFeedPage::postClickedWithGeometry(QString _t1, QRect _t2, QPixmap _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
