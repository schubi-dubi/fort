#include "osutil.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QLoggingCategory>
#include <QProcess>
#include <QUrl>
#include <QWidget>

#define WIN32_LEAN_AND_MEAN
#include <qt_windows.h>

#include <lmcons.h>
#include <mmsystem.h>

#include "fileutil.h"
#include "processinfo.h"

namespace {

const QLoggingCategory LC("util.osUtil");

}

QString OsUtil::pidToPath(quint32 pid, bool isKernelPath)
{
    const ProcessInfo pi(pid);
    return pi.path(isKernelPath);
}

bool OsUtil::openFolder(const QString &filePath)
{
    const QString nativePath = QDir::toNativeSeparators(filePath);

    return QProcess::execute("explorer.exe", { "/select,", nativePath }) == 0;
}

bool OsUtil::openUrl(const QUrl &url)
{
    return QDesktopServices::openUrl(url);
}

bool OsUtil::openUrlOrFolder(const QString &path)
{
    const QUrl url = QUrl::fromUserInput(path);

    if (url.isLocalFile()) {
        const QFileInfo fi(path);
        if (!fi.isDir()) {
            return OsUtil::openFolder(path);
        }
        // else open the folder's content
    }

    return openUrl(url);
}

bool OsUtil::openIpLocationUrl(const QString &ip)
{
    return openUrl("https://www.iplocation.net/ip-lookup?query=" + ip);
}

void *OsUtil::createMutex(const char *name, bool &isSingleInstance)
{
    void *h = CreateMutexA(nullptr, FALSE, name);
    isSingleInstance = h && GetLastError() != ERROR_ALREADY_EXISTS;
    return h;
}

void OsUtil::closeMutex(void *mutexHandle)
{
    CloseHandle(mutexHandle);
}

quint32 OsUtil::lastErrorCode()
{
    return GetLastError();
}

QString OsUtil::errorMessage(quint32 errorCode)
{
    LPWSTR buf = nullptr;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                    | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode, 0, (LPWSTR) &buf, 0, nullptr);

    if (!buf) {
        return QString("System Error %1").arg(errorCode);
    }

    const QString text = QString::fromUtf16((const char16_t *) buf).trimmed();
    LocalFree(buf);
    return text;
}

QString OsUtil::userName()
{
    wchar_t buf[UNLEN + 1];
    DWORD len = UNLEN + 1;
    if (GetUserNameW(buf, &len)) {
        return QString::fromWCharArray(buf, int(len) - 1); // skip terminationg null char.
    }
    return QString();
}

bool OsUtil::isUserAdmin()
{
    SID_IDENTIFIER_AUTHORITY idAuth = SECURITY_NT_AUTHORITY;
    PSID adminGroup;
    BOOL res = AllocateAndInitializeSid(&idAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup);
    if (res) {
        if (!CheckTokenMembership(nullptr, adminGroup, &res)) {
            res = false;
        }
        FreeSid(adminGroup);
    }

    return res;
}

bool OsUtil::beep(BeepType type)
{
    return MessageBeep(type);
}

bool OsUtil::playSound(SoundType /*type*/)
{
    constexpr DWORD flags = SND_APPLICATION | SND_ALIAS | SND_ASYNC | SND_SENTRY;

    return PlaySoundA("MessageNudge", nullptr, flags);
}

bool OsUtil::setCurrentThreadName(const QString &name)
{
    if (name.isEmpty())
        return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    const HRESULT hr = SetThreadDescription(GetCurrentThread(), (PCWSTR) name.utf16());
    return SUCCEEDED(hr);
#else
    return false;
#endif
}

void OsUtil::setThreadIsBusy(bool on)
{
    // Works correct only on Windows 11+
    SetThreadExecutionState(ES_CONTINUOUS | (on ? ES_SYSTEM_REQUIRED : 0));
}

bool OsUtil::allowOtherForegroundWindows()
{
    return AllowSetForegroundWindow(ASFW_ANY);
}

bool OsUtil::excludeWindowFromCapture(QWidget *window, bool on)
{
    return SetWindowDisplayAffinity((HWND) window->winId(), on ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
}

bool OsUtil::registerAppRestart()
{
    return SUCCEEDED(RegisterApplicationRestart(L"--launch", RESTART_NO_CRASH | RESTART_NO_REBOOT));
}

void OsUtil::beginRestartClients()
{
    const auto appPath = FileUtil::pathSlash(FileUtil::appBinLocation());

    FileUtil::writeFileData(appPath + "inst.tmp", {});

    // Create a restart script
    {
        const auto restartScriptPath = appPath + "restart.bat";

        FileUtil::removeFile(restartScriptPath);
        FileUtil::copyFile(":/scripts/restart.bat", restartScriptPath);
        QFile::setPermissions(restartScriptPath, QFile::WriteOwner);
    }
}

void OsUtil::endRestartClients()
{
    const auto appPath = FileUtil::pathSlash(FileUtil::appBinLocation());

    FileUtil::removeFile(appPath + "inst.tmp");
}

void OsUtil::restartClient()
{
    const QFileInfo fi(QCoreApplication::applicationFilePath());

    QString command;
    if (FileUtil::fileExists("restart.bat")) {
        command = "start /min cmd /c restart.bat";
    } else {
        command = QString("ping -n 4 127.0.0.1 >NUL"
                          " & if not exist inst.tmp start %1 --launch")
                          .arg(fi.fileName());
    }

    runCommand(command, /*workingDir=*/fi.path());

    quit("client required restart");
}

void OsUtil::startService(const QString &serviceName)
{
    const auto command = QString("ping -n 2 127.0.0.1 >NUL"
                                 " & sc start %1")
                                 .arg(serviceName);

    runCommand(command);
}

void OsUtil::restart()
{
    const QString appFilePath = QCoreApplication::applicationFilePath();

    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // remove a program path

    qCDebug(LC) << "restart:" << appFilePath << args;

    qApp->connect(qApp, &QObject::destroyed, [=] { QProcess::startDetached(appFilePath, args); });

    quit("required restart");
}

void OsUtil::quit(const QString &reason)
{
    qCDebug(LC) << "Quit due" << reason;

    QCoreApplication::quit();
}

bool OsUtil::runCommand(const QString &command, const QString &workingDir)
{
    const QString scriptPath = qEnvironmentVariable("ComSpec", "cmd.exe");

    const QStringList args = { "/c", command };

    qCDebug(LC) << "Run command:" << scriptPath << args;

    return QProcess::startDetached(scriptPath, args, workingDir);
}
