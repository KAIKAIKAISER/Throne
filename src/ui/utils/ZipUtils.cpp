#include "include/ui/utils/ZipUtils.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

bool ZipUtils::extractZip(const QString &zipFilePath, const QString &destDirPath, QString &errorMsg) {
    QFileInfo zipInfo(zipFilePath);
    if (!zipInfo.exists() || !zipInfo.isFile()) {
        errorMsg = QStringLiteral("ZIP file does not exist: ") + zipFilePath;
        return false;
    }

    QDir destDir(destDirPath);
    if (!destDir.exists() && !destDir.mkpath(".")) {
        errorMsg = QStringLiteral("Failed to create destination directory: ") + destDirPath;
        return false;
    }

#ifdef Q_OS_WIN
    // 1. Try tar.exe (built-in on Windows 10 17063+ and Windows 11)
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.closeWriteChannel();
        QStringList args;
        args << "-xf" << QDir::toNativeSeparators(zipFilePath) << "-C" << QDir::toNativeSeparators(destDirPath);
        proc.start("tar.exe", args);
        if (proc.waitForFinished(10000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            return true;
        }
    }

    // 2. Fallback to PowerShell Expand-Archive
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.closeWriteChannel();
        QStringList args;
        args << "-NoProfile" << "-NonInteractive" << "-Command"
             << QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                    .arg(QDir::toNativeSeparators(zipFilePath))
                    .arg(QDir::toNativeSeparators(destDirPath));
        proc.start("powershell.exe", args);
        if (proc.waitForFinished(15000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            return true;
        }
    }
#else
    // 1. Try unzip on Unix/macOS
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.closeWriteChannel();
        QStringList args;
        args << "-q" << "-o" << zipFilePath << "-d" << destDirPath;
        proc.start("unzip", args);
        if (proc.waitForFinished(10000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            return true;
        }
    }

    // 2. Try tar on Unix/macOS
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.closeWriteChannel();
        QStringList args;
        args << "-xf" << zipFilePath << "-C" << destDirPath;
        proc.start("tar", args);
        if (proc.waitForFinished(10000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            return true;
        }
    }
#endif

    errorMsg = QStringLiteral("Failed to extract ZIP file. Ensure tar or powershell is available.");
    return false;
}

bool ZipUtils::copyRecursively(const QString &srcPath, const QString &dstPath) {
    QDir srcDir(srcPath);
    if (!srcDir.exists()) return false;
    QDir().mkpath(dstPath);

    const auto entries = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &info : entries) {
        const QString srcItem = info.filePath();
        const QString dstItem = QDir(dstPath).filePath(info.fileName());
        if (info.isDir()) {
            if (!copyRecursively(srcItem, dstItem)) return false;
        } else {
            QFile::remove(dstItem);
            if (!QFile::copy(srcItem, dstItem)) return false;
        }
    }
    return true;
}
