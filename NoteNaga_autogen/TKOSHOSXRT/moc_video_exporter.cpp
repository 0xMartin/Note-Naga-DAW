/****************************************************************************
** Meta object code from reading C++ file 'video_exporter.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/video_export/video_exporter.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'video_exporter.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.3. It"
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
struct qt_meta_tag_ZN13VideoExporterE_t {};
} // unnamed namespace

template <> constexpr inline auto VideoExporter::qt_create_metaobjectdata<qt_meta_tag_ZN13VideoExporterE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "VideoExporter",
        "audioProgressUpdated",
        "",
        "percentage",
        "videoProgressUpdated",
        "statusTextChanged",
        "status",
        "finished",
        "error",
        "errorMessage",
        "doExport",
        "onTaskFinished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'audioProgressUpdated'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'videoProgressUpdated'
        QtMocHelpers::SignalData<void(int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'statusTextChanged'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'error'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'doExport'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onTaskFinished'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<VideoExporter, qt_meta_tag_ZN13VideoExporterE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject VideoExporter::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13VideoExporterE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13VideoExporterE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13VideoExporterE_t>.metaTypes,
    nullptr
} };

void VideoExporter::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<VideoExporter *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->audioProgressUpdated((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->videoProgressUpdated((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->statusTextChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->finished(); break;
        case 4: _t->error((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->doExport(); break;
        case 6: _t->onTaskFinished(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (VideoExporter::*)(int )>(_a, &VideoExporter::audioProgressUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (VideoExporter::*)(int )>(_a, &VideoExporter::videoProgressUpdated, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (VideoExporter::*)(const QString & )>(_a, &VideoExporter::statusTextChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (VideoExporter::*)()>(_a, &VideoExporter::finished, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (VideoExporter::*)(const QString & )>(_a, &VideoExporter::error, 4))
            return;
    }
}

const QMetaObject *VideoExporter::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *VideoExporter::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13VideoExporterE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int VideoExporter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void VideoExporter::audioProgressUpdated(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void VideoExporter::videoProgressUpdated(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void VideoExporter::statusTextChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void VideoExporter::finished()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void VideoExporter::error(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
