/****************************************************************************
** Meta object code from reading C++ file 'CustomPushButton.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/Components/CustomPushButton.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CustomPushButton.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSCustomPushButtonENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSCustomPushButtonENDCLASS = QtMocHelpers::stringData(
    "CustomPushButton",
    "radius",
    "backgroundColor",
    "hoverColor",
    "pressColor",
    "textColor",
    "borderColor",
    "borderWidth",
    "isFlat"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSCustomPushButtonENDCLASS_t {
    uint offsetsAndSizes[18];
    char stringdata0[17];
    char stringdata1[7];
    char stringdata2[16];
    char stringdata3[11];
    char stringdata4[11];
    char stringdata5[10];
    char stringdata6[12];
    char stringdata7[12];
    char stringdata8[7];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSCustomPushButtonENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSCustomPushButtonENDCLASS_t qt_meta_stringdata_CLASSCustomPushButtonENDCLASS = {
    {
        QT_MOC_LITERAL(0, 16),  // "CustomPushButton"
        QT_MOC_LITERAL(17, 6),  // "radius"
        QT_MOC_LITERAL(24, 15),  // "backgroundColor"
        QT_MOC_LITERAL(40, 10),  // "hoverColor"
        QT_MOC_LITERAL(51, 10),  // "pressColor"
        QT_MOC_LITERAL(62, 9),  // "textColor"
        QT_MOC_LITERAL(72, 11),  // "borderColor"
        QT_MOC_LITERAL(84, 11),  // "borderWidth"
        QT_MOC_LITERAL(96, 6)   // "isFlat"
    },
    "CustomPushButton",
    "radius",
    "backgroundColor",
    "hoverColor",
    "pressColor",
    "textColor",
    "borderColor",
    "borderWidth",
    "isFlat"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSCustomPushButtonENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       8,   14, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags
       1, QMetaType::Int, 0x00015103, uint(-1), 0,
       2, QMetaType::QColor, 0x00115103, uint(-1), 0,
       3, QMetaType::QColor, 0x00015103, uint(-1), 0,
       4, QMetaType::QColor, 0x00015103, uint(-1), 0,
       5, QMetaType::QColor, 0x00015103, uint(-1), 0,
       6, QMetaType::QColor, 0x00015103, uint(-1), 0,
       7, QMetaType::Int, 0x00015103, uint(-1), 0,
       8, QMetaType::Bool, 0x00015003, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject CustomPushButton::staticMetaObject = { {
    QMetaObject::SuperData::link<QPushButton::staticMetaObject>(),
    qt_meta_stringdata_CLASSCustomPushButtonENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSCustomPushButtonENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSCustomPushButtonENDCLASS_t,
        // property 'radius'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'backgroundColor'
        QtPrivate::TypeAndForceComplete<QColor, std::true_type>,
        // property 'hoverColor'
        QtPrivate::TypeAndForceComplete<QColor, std::true_type>,
        // property 'pressColor'
        QtPrivate::TypeAndForceComplete<QColor, std::true_type>,
        // property 'textColor'
        QtPrivate::TypeAndForceComplete<QColor, std::true_type>,
        // property 'borderColor'
        QtPrivate::TypeAndForceComplete<QColor, std::true_type>,
        // property 'borderWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'isFlat'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CustomPushButton, std::true_type>
    >,
    nullptr
} };

void CustomPushButton::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<CustomPushButton *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->radius(); break;
        case 1: *reinterpret_cast< QColor*>(_v) = _t->backgroundColor(); break;
        case 2: *reinterpret_cast< QColor*>(_v) = _t->hoverColor(); break;
        case 3: *reinterpret_cast< QColor*>(_v) = _t->pressColor(); break;
        case 4: *reinterpret_cast< QColor*>(_v) = _t->textColor(); break;
        case 5: *reinterpret_cast< QColor*>(_v) = _t->borderColor(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->borderWidth(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->isFlat(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<CustomPushButton *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setRadius(*reinterpret_cast< int*>(_v)); break;
        case 1: _t->setBackgroundColor(*reinterpret_cast< QColor*>(_v)); break;
        case 2: _t->setHoverColor(*reinterpret_cast< QColor*>(_v)); break;
        case 3: _t->setPressColor(*reinterpret_cast< QColor*>(_v)); break;
        case 4: _t->setTextColor(*reinterpret_cast< QColor*>(_v)); break;
        case 5: _t->setBorderColor(*reinterpret_cast< QColor*>(_v)); break;
        case 6: _t->setBorderWidth(*reinterpret_cast< int*>(_v)); break;
        case 7: _t->setFlat(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
    (void)_o;
    (void)_id;
    (void)_c;
    (void)_a;
}

const QMetaObject *CustomPushButton::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CustomPushButton::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSCustomPushButtonENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QPushButton::qt_metacast(_clname);
}

int CustomPushButton::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QPushButton::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}
QT_WARNING_POP
