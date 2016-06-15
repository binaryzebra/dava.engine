#include "filemanager.h"
#include "errormessenger.h"
#include <QDesktopServices>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QDirIterator>
#include <QProcess>
#include <functional>

namespace FileManager
{
const QString tempSelfUpdateDir = "selfupdate/";
const QString baseAppDir = "DAVATools/";
const QString tempDir = "temp/";

using EntireList = QList<QPair<QFileInfo, QString>>;

QStringList OwnDirectories()
{
    QString path = GetLauncherDirectory();
    return QStringList() << path + tempSelfUpdateDir
                         << path + baseAppDir
                         << path + tempDir;
}

QStringList DeployDirectories()
{
    return QStringList() << "platforms"
                         << "bearer"
                         << "iconengines"
                         << "imageformats"
                         << "qmltooling"
                         << "translations"
                         << "QtGraphicalEffects"
                         << "QtQuick"
                         << "QtQuick.2";
}

QStringList DeployFiles()
{
    return QStringList() << "Launcher.ilk"
                         << "Launcher.pdb"
                         << "qt.conf";
}

QString GetDocumentsDirectory()
{
    QString docDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/DAVALauncher/";
    MakeDirectory(docDir);
    return docDir;
}

QString GetBaseAppsDirectory()
{
    QString path = GetLauncherDirectory() + baseAppDir;
    MakeDirectory(path);
    return path;
}

QString GetTempDirectory()
{
    QString path = GetLauncherDirectory() + tempDir;
    MakeDirectory(path);
    return path;
}

QString GetSelfUpdateTempDirectory()
{
    QString path = GetLauncherDirectory() + tempSelfUpdateDir;
    MakeDirectory(path);
    return path;
}

QString GetTempDownloadFilePath()
{
    return GetTempDirectory() + "archive.zip";
}

QString GetLauncherDirectory()
{
    QString path =  
#if defined(Q_OS_WIN)
    qApp->applicationDirPath();
#elif defined(Q_OS_MAC)
    qApp->applicationDirPath().replace("/Contents/MacOS", "");
    path = path.left(path.lastIndexOf('/'));
#endif //platform
    return path + "/";
}

bool CreateFileAndWriteData(const QString& filePath, const QByteArray& data)
{
    QFile file(filePath);
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        if (file.write(data) == data.size())
        {
            return true;
        }
    }
    return false;
}

bool DeleteDirectory(const QString& path)
{
    if (path == "/" || path == "." || path == "..")
    {
        ErrorMessenger::ShowErrorMessage(ErrorMessenger::ERROR_PATH, "trying to remove wrong path! Aborted");
        return false;
    }
    QDir dir(path);
    return dir.removeRecursively();
}

namespace FileManager_local
{
bool MoveEntry(const QFileInfo& fileInfo, const QString& newFilePath)
{
    QString absPath = fileInfo.absoluteFilePath();

    QFileInfo destFi(newFilePath);
    if (destFi.exists())
    {
        if (fileInfo.isDir())
        {
            QDir dir(newFilePath);
            if (!dir.remove("."))
            {
                return false;
            }
        }
        else
        {
            if (!QFile::remove(newFilePath))
            {
                return false;
            }
        }
    }
    if (fileInfo.isDir())
    {
        QDir dir(newFilePath);
        return dir.rename(absPath, newFilePath);
    }
    else
    {
        return QFile::rename(absPath, newFilePath);
    }
}
}

EntireList CreateEntireList(const QString& pathOut, const QString& pathIn)
{
    EntireList entryList;
    QDir outDir(pathOut);
    if (!outDir.exists())
    {
        return entryList;
    }
#ifdef Q_OS_WIN
    QString infoFilePath = outDir.absoluteFilePath("Launcher.packageInfo");
    bool moveFilesFromInfoList = QFile::exists(infoFilePath);
    QStringList archiveFiles;
    if (moveFilesFromInfoList)
    {
        QFile file(infoFilePath);
        if (file.open(QFile::ReadOnly))
        {
            QString data = QString::fromUtf8(file.readAll());
            archiveFiles = data.split('\n', QString::SkipEmptyParts);
        }
        else
        {
            return entryList;
        }
    }
#endif //Q_OS_WIN
    QStringList ownDirs = OwnDirectories();
    QStringList deployDirs = DeployDirectories();
    QStringList deployFiles = DeployFiles();
    QDirIterator di(outDir.path(), QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    while (di.hasNext())
    {
        di.next();
        const QFileInfo& fi = di.fileInfo();
        QString absPath = fi.absoluteFilePath();
        QString relPath = absPath.right(absPath.length() - pathOut.length());
        if (fi.isDir() && ownDirs.contains(absPath + '/'))
        {
            continue;
        }
#ifdef Q_OS_WIN
        if (moveFilesFromInfoList && !archiveFiles.contains(relPath))
        {
            continue;
        }
        else if (!moveFilesFromInfoList)
        {
            //this code need for compability with previous launcher versions
            //we create folder "platforms" manually, so must move it with dlls
            QString fileName = fi.fileName();
            if (fi.isDir())
            {
                if (!deployDirs.contains(fileName))
                {
                    continue;
                }
            }
            else
            {
                QString suffix = fi.suffix();
                //all entries, which are not directories and their suffix are not dll or exe
                if (suffix != "dll"
                    && suffix != "exe"
                    && !deployFiles.contains(fileName))
                {
                    continue;
                }
            }
        }
#elif defined(Q_OS_MAC)
        if (fi.fileName() != "Launcher.app")
        {
            continue;
        }
#endif //platform
        QString newFilePath = pathIn + relPath;
        entryList.append(qMakePair(fi, newFilePath));
    }
    return entryList;
}

bool MoveLauncherRecursively(const QString& pathOut, const QString& pathIn)
{
    EntireList entryList = CreateEntireList(pathOut, pathIn);
    if (entryList.isEmpty())
    {
        return false;
    }
    bool success = true;
    for (const QPair<QFileInfo, QString>& entry : entryList)
    {
        success &= FileManager_local::MoveEntry(entry.first, entry.second);
    }
    return success;
}

void MakeDirectory(const QString& path)
{
    if (!QDir(path).exists())
        QDir().mkpath(path);
}

QString GetApplicationDirectory(const QString& branchID, const QString& appID)
{
    QString path = GetBranchDirectory(branchID) + appID + "/";
    return path;
}

QString GetBranchDirectory(const QString& branchID)
{
    QString path = GetBaseAppsDirectory() + branchID + "/";
    return path;
}
}
