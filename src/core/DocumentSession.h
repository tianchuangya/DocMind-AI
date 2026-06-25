// Minimal DocumentSnapshot compatibility header for the module C demo branch.
// The full DocumentSession implementation lives in module A.  Module C only
// needs a read-only snapshot shape for WritingAssistant::fromSnapshot().
#pragma once

#include <QString>

namespace dmc {

struct DocumentSnapshot {
    QString content;
    QString filePath;
    QString title;
    QString baseDir;
    qint64  renderVersion = 0;
    bool    isModified = false;
};

} // namespace dmc
