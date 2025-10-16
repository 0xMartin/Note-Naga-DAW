/****************************************************************************
** Meta object code from reading C++ file 'main_window.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/gui/main_window.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'main_window.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "set_auto_follow",
        "",
        "checked",
        "toggle_play",
        "on_playing_state_changed",
        "playing",
        "goto_start",
        "goto_end",
        "onControlBarPositionClicked",
        "seconds",
        "tick_position",
        "open_midi",
        "export_midi",
        "reset_all_colors",
        "randomize_all_colors",
        "about_dialog",
        "reset_layout",
        "show_hide_dock",
        "name",
        "export_video",
        "util_quantize",
        "util_humanize",
        "util_transpose",
        "util_set_velocity",
        "util_scale_velocity",
        "util_set_duration",
        "util_scale_duration",
        "util_legato",
        "util_staccato",
        "util_invert",
        "util_retrograde",
        "util_delete_overlapping",
        "util_scale_timing"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'set_auto_follow'
        QtMocHelpers::SlotData<void(bool)>(1, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 3 },
        }}),
        // Slot 'toggle_play'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'on_playing_state_changed'
        QtMocHelpers::SlotData<void(bool)>(5, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 6 },
        }}),
        // Slot 'goto_start'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'goto_end'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onControlBarPositionClicked'
        QtMocHelpers::SlotData<void(float, int)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Float, 10 }, { QMetaType::Int, 11 },
        }}),
        // Slot 'open_midi'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'export_midi'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'reset_all_colors'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'randomize_all_colors'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'about_dialog'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'reset_layout'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'show_hide_dock'
        QtMocHelpers::SlotData<void(const QString &, bool)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 19 }, { QMetaType::Bool, 3 },
        }}),
        // Slot 'export_video'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_quantize'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_humanize'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_transpose'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_set_velocity'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_scale_velocity'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_set_duration'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_scale_duration'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_legato'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_staccato'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_invert'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_retrograde'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_delete_overlapping'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'util_scale_timing'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->set_auto_follow((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 1: _t->toggle_play(); break;
        case 2: _t->on_playing_state_changed((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->goto_start(); break;
        case 4: _t->goto_end(); break;
        case 5: _t->onControlBarPositionClicked((*reinterpret_cast< std::add_pointer_t<float>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 6: _t->open_midi(); break;
        case 7: _t->export_midi(); break;
        case 8: _t->reset_all_colors(); break;
        case 9: _t->randomize_all_colors(); break;
        case 10: _t->about_dialog(); break;
        case 11: _t->reset_layout(); break;
        case 12: _t->show_hide_dock((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 13: _t->export_video(); break;
        case 14: _t->util_quantize(); break;
        case 15: _t->util_humanize(); break;
        case 16: _t->util_transpose(); break;
        case 17: _t->util_set_velocity(); break;
        case 18: _t->util_scale_velocity(); break;
        case 19: _t->util_set_duration(); break;
        case 20: _t->util_scale_duration(); break;
        case 21: _t->util_legato(); break;
        case 22: _t->util_staccato(); break;
        case 23: _t->util_invert(); break;
        case 24: _t->util_retrograde(); break;
        case 25: _t->util_delete_overlapping(); break;
        case 26: _t->util_scale_timing(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 27)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 27;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 27)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 27;
    }
    return _id;
}
QT_WARNING_POP
