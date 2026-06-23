/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/app/MainWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
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
struct qt_meta_tag_ZN3dmc10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto dmc::MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN3dmc10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "dmc::MainWindow",
        "windowStateChanged",
        "",
        "onNewFile",
        "onOpenFile",
        "onOpenRecentFile",
        "onSave",
        "onSaveAs",
        "onSaveAll",
        "onCloseTab",
        "onCloseAllTabs",
        "onCloseOtherTabs",
        "onUndo",
        "onRedo",
        "onCut",
        "onCopy",
        "onPaste",
        "onSelectAll",
        "onFind",
        "onFindReplace",
        "onGoToLine",
        "onInsertHeading",
        "onInsertBold",
        "onInsertItalic",
        "onInsertCode",
        "onInsertCodeBlock",
        "onInsertLink",
        "onInsertImage",
        "onInsertTable",
        "onInsertTaskList",
        "onInsertBlockquote",
        "onInsertHorizontalRule",
        "onInsertOrderedList",
        "onInsertUnorderedList",
        "onToggleEditorOnly",
        "onToggleSplitView",
        "onTogglePreviewOnly",
        "onToggleTheme",
        "onToggleLineNumbers",
        "onToggleWordWrap",
        "onZoomIn",
        "onZoomOut",
        "onResetZoom",
        "onSettings",
        "onAbout",
        "onCurrentTabChanged",
        "DocumentSession*",
        "session",
        "onCursorInfoChanged",
        "line",
        "col",
        "selLen",
        "onEditorContentModified",
        "onAutoSaveTimeout",
        "updateStatusBar",
        "updatePreview"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'windowStateChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onNewFile'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onOpenFile'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onOpenRecentFile'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSave'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSaveAs'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSaveAll'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCloseTab'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCloseAllTabs'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCloseOtherTabs'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onUndo'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRedo'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCut'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCopy'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPaste'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSelectAll'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onFind'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onFindReplace'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onGoToLine'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertHeading'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertBold'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertItalic'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertCode'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertCodeBlock'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertLink'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertImage'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertTable'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertTaskList'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertBlockquote'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertHorizontalRule'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertOrderedList'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInsertUnorderedList'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onToggleEditorOnly'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onToggleSplitView'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTogglePreviewOnly'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onToggleTheme'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onToggleLineNumbers'
        QtMocHelpers::SlotData<void()>(38, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onToggleWordWrap'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onZoomIn'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onZoomOut'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onResetZoom'
        QtMocHelpers::SlotData<void()>(42, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSettings'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAbout'
        QtMocHelpers::SlotData<void()>(44, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCurrentTabChanged'
        QtMocHelpers::SlotData<void(DocumentSession *)>(45, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 46, 47 },
        }}),
        // Slot 'onCursorInfoChanged'
        QtMocHelpers::SlotData<void(int, int, int)>(48, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 49 }, { QMetaType::Int, 50 }, { QMetaType::Int, 51 },
        }}),
        // Slot 'onEditorContentModified'
        QtMocHelpers::SlotData<void()>(52, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAutoSaveTimeout'
        QtMocHelpers::SlotData<void()>(53, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateStatusBar'
        QtMocHelpers::SlotData<void()>(54, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updatePreview'
        QtMocHelpers::SlotData<void()>(55, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN3dmc10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject dmc::MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3dmc10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3dmc10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3dmc10MainWindowE_t>.metaTypes,
    nullptr
} };

void dmc::MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->windowStateChanged(); break;
        case 1: _t->onNewFile(); break;
        case 2: _t->onOpenFile(); break;
        case 3: _t->onOpenRecentFile(); break;
        case 4: _t->onSave(); break;
        case 5: _t->onSaveAs(); break;
        case 6: _t->onSaveAll(); break;
        case 7: _t->onCloseTab(); break;
        case 8: _t->onCloseAllTabs(); break;
        case 9: _t->onCloseOtherTabs(); break;
        case 10: _t->onUndo(); break;
        case 11: _t->onRedo(); break;
        case 12: _t->onCut(); break;
        case 13: _t->onCopy(); break;
        case 14: _t->onPaste(); break;
        case 15: _t->onSelectAll(); break;
        case 16: _t->onFind(); break;
        case 17: _t->onFindReplace(); break;
        case 18: _t->onGoToLine(); break;
        case 19: _t->onInsertHeading(); break;
        case 20: _t->onInsertBold(); break;
        case 21: _t->onInsertItalic(); break;
        case 22: _t->onInsertCode(); break;
        case 23: _t->onInsertCodeBlock(); break;
        case 24: _t->onInsertLink(); break;
        case 25: _t->onInsertImage(); break;
        case 26: _t->onInsertTable(); break;
        case 27: _t->onInsertTaskList(); break;
        case 28: _t->onInsertBlockquote(); break;
        case 29: _t->onInsertHorizontalRule(); break;
        case 30: _t->onInsertOrderedList(); break;
        case 31: _t->onInsertUnorderedList(); break;
        case 32: _t->onToggleEditorOnly(); break;
        case 33: _t->onToggleSplitView(); break;
        case 34: _t->onTogglePreviewOnly(); break;
        case 35: _t->onToggleTheme(); break;
        case 36: _t->onToggleLineNumbers(); break;
        case 37: _t->onToggleWordWrap(); break;
        case 38: _t->onZoomIn(); break;
        case 39: _t->onZoomOut(); break;
        case 40: _t->onResetZoom(); break;
        case 41: _t->onSettings(); break;
        case 42: _t->onAbout(); break;
        case 43: _t->onCurrentTabChanged((*reinterpret_cast<std::add_pointer_t<DocumentSession*>>(_a[1]))); break;
        case 44: _t->onCursorInfoChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 45: _t->onEditorContentModified(); break;
        case 46: _t->onAutoSaveTimeout(); break;
        case 47: _t->updateStatusBar(); break;
        case 48: _t->updatePreview(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MainWindow::*)()>(_a, &MainWindow::windowStateChanged, 0))
            return;
    }
}

const QMetaObject *dmc::MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *dmc::MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3dmc10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int dmc::MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 49)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 49;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 49)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 49;
    }
    return _id;
}

// SIGNAL 0
void dmc::MainWindow::windowStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
