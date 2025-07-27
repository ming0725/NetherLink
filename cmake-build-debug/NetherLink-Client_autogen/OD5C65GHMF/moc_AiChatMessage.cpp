/****************************************************************************
** Meta object code from reading C++ file 'AiChatMessage.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/View/AiChat/AiChatMessage.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AiChatMessage.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSAiChatMessageENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSAiChatMessageENDCLASS = QtMocHelpers::stringData(
    "AiChatMessage",
    "Role",
    "User",
    "AI"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSAiChatMessageENDCLASS_t {
    uint offsetsAndSizes[8];
    char stringdata0[14];
    char stringdata1[5];
    char stringdata2[5];
    char stringdata3[3];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSAiChatMessageENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSAiChatMessageENDCLASS_t qt_meta_stringdata_CLASSAiChatMessageENDCLASS = {
    {
        QT_MOC_LITERAL(0, 13),  // "AiChatMessage"
        QT_MOC_LITERAL(14, 4),  // "Role"
        QT_MOC_LITERAL(19, 4),  // "User"
        QT_MOC_LITERAL(24, 2)   // "AI"
    },
    "AiChatMessage",
    "Role",
    "User",
    "AI"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSAiChatMessageENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       1,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,    2,   19,

 // enum data: key, value
       2, uint(AiChatMessage::User),
       3, uint(AiChatMessage::AI),

       0        // eod
};

Q_CONSTINIT const QMetaObject AiChatMessage::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_CLASSAiChatMessageENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSAiChatMessageENDCLASS,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSAiChatMessageENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AiChatMessage, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP
