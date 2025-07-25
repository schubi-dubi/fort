#ifndef PROGRAMSCONTROLLER_H
#define PROGRAMSCONTROLLER_H

#include "programeditcontroller.h"

class AppInfoCache;
class AppListModel;

class ProgramsController : public ProgramEditController
{
    Q_OBJECT

public:
    explicit ProgramsController(QObject *parent = nullptr);

    AppInfoCache *appInfoCache() const;
    AppListModel *appListModel() const { return m_appListModel; }

    void initialize();

public slots:
    void updateAppsBlocked(const QVector<qint64> &appIdList, bool blocked, bool killProcess);
    void updateAppsTimer(const QVector<qint64> &appIdList, int minutes);
    void deleteApps(const QVector<qint64> &appIdList);
    void deleteAlertedApps();
    void clearAlerts();
    void purgeApps();

private:
    AppListModel *m_appListModel = nullptr;
};

#endif // PROGRAMSCONTROLLER_H
