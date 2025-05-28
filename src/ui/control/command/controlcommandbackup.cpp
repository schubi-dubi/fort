#include "controlcommandbackup.h"

#include <conf/confmanager.h>
#include <stat/statmanager.h>
#include <util/ioc/ioccontainer.h>

namespace {

enum BackupAction : quint32 {
    BackupActionNone = 0,
    BackupActionExport = (1 << 0),
    BackupActionImport = (1 << 1),
};

BackupAction backupActionByText(const QString &commandText)
{
    if (commandText == "export")
        return BackupActionExport;

    if (commandText == "import")
        return BackupActionImport;

    return BackupActionNone;
}

bool processCommandBackupAction(BackupAction backupAction, const QString &dirPath)
{
    auto statManager = IoC<StatManager>();
    auto confManager = IoC<ConfManager>();

    switch (backupAction) {
    case BackupActionExport: {
        statManager->exportBackup(dirPath);

        return confManager->exportBackup(dirPath);
    }
    case BackupActionImport: {
        statManager->importBackup(dirPath);

        return confManager->importBackup(dirPath);
    }
    default:
        return false;
    }
}

}

bool ControlCommandBackup::processCommand(const ProcessCommandArgs &p, ProcessCommandResult &r)
{
    const BackupAction backupAction = backupActionByText(p.args.value(0).toString());
    if (backupAction == BackupActionNone) {
        r.errorMessage = "Usage: backup export|import <dir-path>";
        return false;
    }

    if (!checkCommandActionPassword(r, backupAction))
        return false;

    const QString dirPath = p.args.value(1).toString();

    const bool ok = processCommandBackupAction(backupAction, dirPath);

    uncheckCommandActionPassword();

    return ok;
}
