/****************************************************************************
** Meta object code from reading C++ file 'SearchResultList.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/View/Friend/SearchResultList.h"
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
#error "The header file 'SearchResultList.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSSearchResultListENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSSearchResultListENDCLASS = QtMocHelpers::stringData(
    "SearchResultList",
    "onSearchTimeout",
    "",
    "onMoreUsersClicked",
    "onMoreGroupsClicked",
    "onNetworkReplyFinished"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSSearchResultListENDCLASS_t {
    uint offsetsAndSizes[12];
    char stringdata0[17];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[19];
    char stringdata4[20];
    char stringdata5[23];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSSearchResultListENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSSearchResultListENDCLASS_t qt_meta_stringdata_CLASSSearchResultListENDCLASS = {
    {
        QT_MOC_LITERAL(0, 16),  // "SearchResultList"
        QT_MOC_LITERAL(17, 15),  // "onSearchTimeout"
        QT_MOC_LITERAL(33, 0),  // ""
        QT_MOC_LITERAL(34, 18),  // "onMoreUsersClicked"
        QT_MOC_LITERAL(53, 19),  // "onMoreGroupsClicked"
        QT_MOC_LITERAL(73, 22)   // "onNetworkReplyFinished"
    },
    "SearchResultList",
    "onSearchTimeout",
    "",
    "onMoreUsersClicked",
    "onMoreGroupsClicked",
    "onNetworkReplyFinished"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSSearchResultListENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   38,    2, 0x08,    1 /* Private */,
       3,    0,   39,    2, 0x08,    2 /* Private */,
       4,    0,   40,    2, 0x08,    3 /* Private */,
       5,    0,   41,    2, 0x08,    4 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject SearchResultList::staticMetaObject = { {
    QMetaObject::SuperData::link<CustomScrollArea::staticMetaObject>(),
    qt_meta_stringdata_CLASSSearchResultListENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSSearchResultListENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSSearchResultListENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<SearchResultList, std::true_type>,
        // method 'onSearchTimeout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onMoreUsersClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onMoreGroupsClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNetworkReplyFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void SearchResultList::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SearchResultList *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onSearchTimeout(); break;
        case 1: _t->onMoreUsersClicked(); break;
        case 2: _t->onMoreGroupsClicked(); break;
        case 3: _t->onNetworkReplyFinished(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *SearchResultList::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SearchResultList::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSSearchResultListENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return CustomScrollArea::qt_metacast(_clname);
}

int SearchResultList::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = CustomScrollArea::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}
QT_WARNING_POP
