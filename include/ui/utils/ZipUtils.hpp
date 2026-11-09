#pragma once

#include <QString>

namespace ZipUtils {
    bool extractZip(const QString &zipFilePath, const QString &destDirPath, QString &errorMsg);
    bool copyRecursively(const QString &srcPath, const QString &dstPath);
} // namespace ZipUtils
