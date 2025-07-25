#include "trayicon.h"

#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QTimer>

#include <conf/addressgroup.h>
#include <conf/appgroup.h>
#include <conf/confappmanager.h>
#include <conf/confmanager.h>
#include <conf/confrulemanager.h>
#include <conf/firewallconf.h>
#include <driver/drivermanager.h>
#include <form/controls/clickablemenu.h>
#include <form/controls/controlutil.h>
#include <form/controls/mainwindow.h>
#include <form/form_types.h>
#include <fortsettings.h>
#include <manager/hotkeymanager.h>
#include <manager/windowmanager.h>
#include <user/iniuser.h>
#include <util/guiutil.h>
#include <util/iconcache.h>
#include <util/osutil.h>
#include <util/window/widgetwindow.h>

#include "traycontroller.h"

namespace {

constexpr int MAX_FKEY_COUNT = 12;
constexpr int MAX_RULE_ACTIONS_COUNT = 8;

const QString eventSingleClick = QStringLiteral("singleClick");
const QString eventCtrlSingleClick = QStringLiteral("ctrlSingleClick");
const QString eventAltSingleClick = QStringLiteral("altSingleClick");
const QString eventDoubleClick = QStringLiteral("doubleClick");
const QString eventMiddleClick = QStringLiteral("middleClick");
const QString eventRightClick = QStringLiteral("rightClick");

const QString actionShowHome = QStringLiteral("Home");
const QString actionShowPrograms = QStringLiteral("Programs");
const QString actionShowProgramsOrAlert = QStringLiteral("ProgramsOrAlert");
const QString actionShowOptions = QStringLiteral("Options");
const QString actionShowStatistics = QStringLiteral("Statistics");
const QString actionShowTrafficGraph = QStringLiteral("TrafficGraph");
const QString actionSwitchFilterEnabled = QStringLiteral("FilterEnabled");
const QString actionSwitchSnoozeAlerts = QStringLiteral("SnoozeAlerts");
const QString actionShowBlockTrafficMenu = QStringLiteral("BlockTrafficMenu");
const QString actionShowFilterModeMenu = QStringLiteral("FilterModeMenu");
const QString actionShowTrayMenu = QStringLiteral("TrayMenu");
const QString actionIgnore = QStringLiteral("Ignore");

QString clickNameByType(TrayIcon::ClickType clickType)
{
    switch (clickType) {
    case TrayIcon::SingleClick:
        return eventSingleClick;
    case TrayIcon::CtrlSingleClick:
        return eventCtrlSingleClick;
    case TrayIcon::AltSingleClick:
        return eventAltSingleClick;
    case TrayIcon::DoubleClick:
        return eventDoubleClick;
    case TrayIcon::MiddleClick:
        return eventMiddleClick;
    case TrayIcon::RightClick:
        return eventRightClick;
    default:
        return QString();
    }
}

QString actionNameByType(TrayIcon::ActionType actionType)
{
    static const QString actionNames[] = {
        actionShowHome,
        actionShowPrograms,
        actionShowProgramsOrAlert,
        actionShowOptions,
        actionShowStatistics,
        actionShowTrafficGraph,
        actionSwitchFilterEnabled,
        actionSwitchSnoozeAlerts,
        actionShowBlockTrafficMenu,
        actionShowFilterModeMenu,
        actionShowTrayMenu,
        actionIgnore,
    };

    if (actionType > TrayIcon::ActionNone && actionType < TrayIcon::ActionTypeCount) {
        return actionNames[actionType];
    }

    return {};
}

TrayIcon::ActionType actionTypeByName(const QString &name)
{
    static const QHash<QString, TrayIcon::ActionType> actionTypeNamesMap = {
        { actionShowHome, TrayIcon::ActionShowHome },
        { actionShowPrograms, TrayIcon::ActionShowPrograms },
        { actionShowProgramsOrAlert, TrayIcon::ActionShowProgramsOrAlert },
        { actionShowOptions, TrayIcon::ActionShowOptions },
        { actionShowStatistics, TrayIcon::ActionShowStatistics },
        { actionShowTrafficGraph, TrayIcon::ActionShowTrafficGraph },
        { actionSwitchFilterEnabled, TrayIcon::ActionSwitchFilterEnabled },
        { actionSwitchSnoozeAlerts, TrayIcon::ActionSwitchSnoozeAlerts },
        { actionShowBlockTrafficMenu, TrayIcon::ActionShowBlockTrafficMenu },
        { actionShowFilterModeMenu, TrayIcon::ActionShowFilterModeMenu },
        { actionShowTrayMenu, TrayIcon::ActionShowTrayMenu },
        { actionIgnore, TrayIcon::ActionIgnore }
    };

    return name.isEmpty() ? TrayIcon::ActionNone
                          : actionTypeNamesMap.value(name, TrayIcon::ActionNone);
}

TrayIcon::ActionType defaultActionTypeByClick(TrayIcon::ClickType clickType)
{
    switch (clickType) {
    case TrayIcon::SingleClick:
        return TrayIcon::ActionShowProgramsOrAlert;
    case TrayIcon::CtrlSingleClick:
        return TrayIcon::ActionShowOptions;
    case TrayIcon::AltSingleClick:
        return TrayIcon::ActionIgnore;
    case TrayIcon::DoubleClick:
        return TrayIcon::ActionIgnore;
    case TrayIcon::MiddleClick:
        return TrayIcon::ActionShowStatistics;
    case TrayIcon::RightClick:
        return TrayIcon::ActionShowTrayMenu;
    default:
        return TrayIcon::ActionNone;
    }
}

TrayIcon::ClickType clickTypeByMouse(Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    switch (button) {
    case Qt::LeftButton: {
        if (modifiers & Qt::ControlModifier) {
            return TrayIcon::CtrlSingleClick;
        } else if (modifiers & Qt::AltModifier) {
            return TrayIcon::AltSingleClick;
        }
    } break;
    case Qt::MiddleButton: {
        return TrayIcon::MiddleClick;
    } break;
    case Qt::RightButton: {
        return TrayIcon::RightClick;
    } break;
    }

    return TrayIcon::SingleClick;
}

void setActionCheckable(QAction *action, bool checked = false, const QObject *receiver = nullptr,
        const char *member = nullptr)
{
    action->setCheckable(true);
    action->setChecked(checked);

    if (receiver) {
        QObject::connect(action, SIGNAL(toggled(bool)), receiver, member);
    }
}

struct AddActionArgs
{
    const QString &iconPath;
    const QObject *receiver = nullptr;
    const char *member = nullptr;
    QVariant userData;
    bool checkable = false;
    bool checked = false;
};

QAction *addAction(QWidget *widget, const AddActionArgs &a)
{
    auto action = new QAction(widget);

    if (a.userData.isValid()) {
        action->setData(a.userData);
    }

    if (!a.iconPath.isEmpty()) {
        action->setIcon(IconCache::icon(a.iconPath));
    }
    if (a.receiver) {
        QObject::connect(action, SIGNAL(triggered(bool)), a.receiver, a.member);
    }
    if (a.checkable) {
        setActionCheckable(action, a.checked);
    }

    widget->addAction(action);

    return action;
}

bool checkAlertFilterMode(FirewallConf *conf, IniUser *iniUser)
{
    switch (conf->filterMode()) {
    case FirewallConf::ModeAutoLearn:
        return iniUser->progAlertWindowAutoLearn();
    case FirewallConf::ModeAskToConnect:
        return true;
    case FirewallConf::ModeBlockAll:
        return iniUser->progAlertWindowBlockAll();
    case FirewallConf::ModeAllowAll:
        return iniUser->progAlertWindowAllowAll();
    default:
        return false;
    }
}

}

TrayIcon::TrayIcon(QObject *parent) : QSystemTrayIcon(parent), m_ctrl(new TrayController(this))
{
    setupUi();
    setupController();

    connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onTrayActivated);

    connect(confManager(), &ConfManager::confChanged, this, &TrayIcon::updateTrayMenu);
    connect(confManager(), &ConfManager::iniUserChanged, this, &TrayIcon::setupByIniUser);

    connect(confAppManager(), &ConfAppManager::appAlerted, this, &TrayIcon::onAppAlerted,
            Qt::QueuedConnection);

    connect(confRuleManager(), &ConfRuleManager::trayMenuUpdated, this,
            &TrayIcon::updateRuleActions, Qt::QueuedConnection);

    connect(driverManager(), &DriverManager::isDeviceOpenedChanged, this,
            &TrayIcon::updateTrayIconShape);
}

void TrayIcon::setIconPath(const QString &v)
{
    if (m_iconPath != v) {
        m_iconPath = v;
        emit iconPathChanged(v);
    }
}

FortSettings *TrayIcon::settings() const
{
    return ctrl()->settings();
}

ConfManager *TrayIcon::confManager() const
{
    return ctrl()->confManager();
}

ConfAppManager *TrayIcon::confAppManager() const
{
    return ctrl()->confAppManager();
}

ConfRuleManager *TrayIcon::confRuleManager() const
{
    return ctrl()->confRuleManager();
}

FirewallConf *TrayIcon::conf() const
{
    return ctrl()->conf();
}

IniOptions *TrayIcon::ini() const
{
    return ctrl()->ini();
}

IniUser *TrayIcon::iniUser() const
{
    return ctrl()->iniUser();
}

HotKeyManager *TrayIcon::hotKeyManager() const
{
    return ctrl()->hotKeyManager();
}

DriverManager *TrayIcon::driverManager() const
{
    return ctrl()->driverManager();
}

WindowManager *TrayIcon::windowManager() const
{
    return ctrl()->windowManager();
}

void TrayIcon::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (WindowManager::activateModalWidget())
        return;

    switch (reason) {
    case QSystemTrayIcon::Trigger: {
        onTrayActivatedByTrigger();
    } break;
    case QSystemTrayIcon::DoubleClick: {
        onTrayActivatedByClick(DoubleClick);
    } break;
    case QSystemTrayIcon::MiddleClick: {
        onTrayActivatedByClick(MiddleClick);
    } break;
    case QSystemTrayIcon::Context: {
        onTrayActivatedByClick(RightClick);
    } break;
    default:
        break;
    }
}

void TrayIcon::onAppAlerted(bool alerted)
{
    updateTrayIcon(alerted);

    if (alerted) {
        sendAlertMessage();
    }
}

void TrayIcon::updateTrayIcon(bool alerted)
{
    if (m_alerted == alerted)
        return;

    m_alerted = alerted;
    m_animatedAlert = false;

    if (m_alerted && !iniUser()->trayShowAlert())
        return;

    updateAlertTimer();
    updateTrayIconShape();
}

void TrayIcon::showTrayMenu(const QPoint &pos)
{
    m_menu->popup(pos);
}

void TrayIcon::hideTrayMenuLater()
{
    QMetaObject::invokeMethod(m_menu, &QMenu::hide, Qt::QueuedConnection);
}

void TrayIcon::updateTrayMenu(bool onlyFlags)
{
    if (!onlyFlags) {
        updateAppGroupActions();
    }

    updateTrayMenuFlags();
    updateTrayIconShape();
    updateClickActions();
    updateActionHotKeys();
    updateHotKeys();
}

void TrayIcon::quitProgram()
{
    if (iniUser()->confirmQuit()) {
        windowManager()->showConfirmBox(
                [&] { windowManager()->quit(); }, tr("Are you sure you want to quit the program?"));
    } else {
        windowManager()->quit();
    }
}

void TrayIcon::setupByIniUser(const IniUser & /*ini*/, bool onlyFlags)
{
    updateTrayMenu(onlyFlags);
}

void TrayIcon::switchTrayMenu(bool /*checked*/)
{
    showTrayMenu(QCursor::pos());
}

void TrayIcon::switchBlockTrafficMenu(bool /*checked*/)
{
    m_blockTrafficMenu->popup(QCursor::pos());
}

void TrayIcon::switchFilterModeMenu(bool /*checked*/)
{
    m_filterModeMenu->popup(QCursor::pos());
}

void TrayIcon::setupController()
{
    connect(windowManager(), &WindowManager::windowVisibilityChanged, this,
            &TrayIcon::onWindowVisibilityChanged);

    connect(settings(), &FortSettings::passwordCheckedChanged, this,
            &TrayIcon::updateTrayMenuFlags);

    connect(ctrl(), &TrayController::retranslateUi, this, &TrayIcon::retranslateUi);

    retranslateUi();
}

void TrayIcon::retranslateUi()
{
    m_homeAction->setText(tr("My Fort"));
    m_programsAction->setText(tr("Programs"));
    m_optionsMenu->setTitle(tr("Options"));
    m_optionsAction->setText(tr("Options"));
    m_rulesAction->setText(tr("Rules"));
    m_zonesAction->setText(tr("Zones"));
    m_groupsAction->setText(tr("Groups"));
    m_servicesAction->setText(tr("Services"));
    m_statisticsAction->setText(tr("Statistics"));
    m_graphAction->setText(tr("Traffic Graph"));

    m_filterEnabledAction->setText(tr("Filter Enabled"));
    m_snoozeAlertsAction->setText(tr("Snooze Alerts"));

    m_blockTrafficMenu->setTitle(tr("Block Traffic"));
    retranslateBlockTrafficActions();

    m_filterModeMenu->setTitle(tr("Filter Mode"));
    retranslateFilterModeActions();

    m_quitAction->setText(tr("Quit"));
}

void TrayIcon::retranslateBlockTrafficActions()
{
    int index = 0;
    for (const QString &name : FirewallConf::blockTrafficNames()) {
        QAction *a = m_blockTrafficActions->actions().at(index++);
        a->setText(name);
    }
}

void TrayIcon::retranslateFilterModeActions()
{
    int index = 0;
    for (const QString &name : FirewallConf::filterModeNames()) {
        QAction *a = m_filterModeActions->actions().at(index++);
        a->setText(name);
    }
}

void TrayIcon::setupUi()
{
    setupTrayMenu();
    updateTrayMenu();

    updateRuleActions();

    this->setContextMenu(m_menu);
    this->setToolTip(QApplication::applicationDisplayName());
}

void TrayIcon::setupTrayMenu()
{
    m_menu = new ClickableMenu(windowManager()->mainWindow());

    connect(m_menu, &QMenu::aboutToShow, this, [&] {
        if (WindowManager::activateModalWidget()) {
            hideTrayMenuLater();
        }
    });

    setupTrayMenuTopActions();

    m_menu->addSeparator();
    setupTrayMenuGroupActions();

    m_menu->addSeparator();
    setupTrayMenuRuleActions();

    m_menu->addSeparator();
    setupTrayMenuBottomActions();
}

void TrayIcon::setupTrayMenuTopActions()
{
    m_homeAction =
            addAction(m_menu, { ":/icons/fort.png", this, SLOT(onShowWindowAction()), WindowHome });
    addHotKey(m_homeAction, HotKey::home);

    m_programsAction = addAction(m_menu,
            { ":/icons/application.png", this, SLOT(onShowWindowAction()), WindowPrograms });
    addHotKey(m_programsAction, HotKey::programs);

    m_programsOrAlertAction =
            addAction(m_menu, { QString(), this, SLOT(showProgramsOrAlertWindow()) });
    m_programsOrAlertAction->setVisible(false);

    setupTrayMenuOptions();
    m_menu->addMenu(m_optionsMenu);

    m_statisticsAction = addAction(m_menu,
            { ":/icons/chart_bar.png", this, SLOT(onShowWindowAction()), WindowStatistics });
    addHotKey(m_statisticsAction, HotKey::statistics);

    m_graphAction = addAction(m_menu,
            { ":/icons/action_log.png", this, SLOT(onShowWindowAction()), WindowGraph,
                    /*checkable=*/true, windowManager()->isWindowOpen(WindowGraph) });
    addHotKey(m_graphAction, HotKey::graph);

    m_menu->addSeparator();

    m_filterEnabledAction = addAction(m_menu,
            { QString(), this, SLOT(switchTrayFlag(bool)), ActionSwitchFilterEnabled,
                    /*checkable=*/true });
    addHotKey(m_filterEnabledAction, HotKey::filter);

    m_snoozeAlertsAction = addAction(m_menu,
            { QString(), this, SLOT(switchTrayFlag(bool)), ActionSwitchSnoozeAlerts,
                    /*checkable=*/true });
    addHotKey(m_snoozeAlertsAction, HotKey::snoozeAlerts);

    m_blockTrafficMenuAction = addAction(m_menu,
            { QString(), this, SLOT(switchBlockTrafficMenu(bool)), ActionShowBlockTrafficMenu });
    m_blockTrafficMenuAction->setVisible(false);

    setupTrayMenuBlockTraffic();
    m_menu->addMenu(m_blockTrafficMenu);

    m_filterModeMenuAction = addAction(m_menu,
            { QString(), this, SLOT(switchFilterModeMenu(bool)), ActionShowFilterModeMenu });
    m_filterModeMenuAction->setVisible(false);

    setupTrayMenuFilterMode();
    m_menu->addMenu(m_filterModeMenu);
}

void TrayIcon::setupTrayMenuOptions()
{
    m_optionsMenu = new QMenu(m_menu);
    m_optionsMenu->setIcon(IconCache::icon(":/icons/cog.png"));

    m_optionsAction = addAction(
            m_optionsMenu, { ":/icons/cog.png", this, SLOT(onShowWindowAction()), WindowOptions });
    addHotKey(m_optionsAction, HotKey::options);

    m_rulesAction = addAction(
            m_optionsMenu, { ":/icons/script.png", this, SLOT(onShowWindowAction()), WindowRules });
    addHotKey(m_rulesAction, HotKey::rules);

    m_zonesAction = addAction(m_optionsMenu,
            { ":/icons/ip_class.png", this, SLOT(onShowWindowAction()), WindowZones });
    addHotKey(m_zonesAction, HotKey::zones);

    m_optionsMenu->addSeparator();

    m_groupsAction = addAction(m_optionsMenu,
            { ":/icons/application_double.png", windowManager(), SLOT(showAppGroupsWindow()) });
    addHotKey(m_groupsAction, HotKey::groups);

    m_servicesAction = addAction(m_optionsMenu,
            { ":/icons/windows-48.png", this, SLOT(onShowWindowAction()), WindowServices });
    addHotKey(m_servicesAction, HotKey::services);

    m_servicesAction->setEnabled(settings()->hasMasterAdmin());
}

void TrayIcon::setupTrayMenuBlockTraffic()
{
    static const char *const blockTrafficIniKeys[] = {
        HotKey::blockTrafficOff,
        HotKey::blockInetTraffic,
        HotKey::blockLanTraffic,
        HotKey::blockInetLanTraffic,
        HotKey::blockTraffic,
    };

    m_blockTrafficMenu = ControlUtil::createMenu(m_menu);

    m_blockTrafficActions = new QActionGroup(m_blockTrafficMenu);

    int index = 0;
    for (const QString &name : FirewallConf::blockTrafficNames()) {
        if (Q_UNLIKELY(index >= std::size(blockTrafficIniKeys)))
            break;

        const char *iniKey = blockTrafficIniKeys[index];

        QAction *a = addAction(m_blockTrafficMenu,
                { /*iconPath=*/ {}, /*receiver=*/nullptr, /*member=*/nullptr, ActionNone,
                        /*checkable=*/true });
        a->setText(name);

        addHotKey(a, iniKey);

        m_blockTrafficActions->addAction(a);
        ++index;
    }

    connect(m_blockTrafficActions, &QActionGroup::triggered, this, &TrayIcon::switchBlockTraffic);
}

void TrayIcon::setupTrayMenuFilterMode()
{
    static const char *const filterModeIniKeys[] = {
        HotKey::filterModeAutoLearn,
        HotKey::filterModeAskToConnect,
        HotKey::filterModeBlock,
        HotKey::filterModeAllow,
        HotKey::filterModeIgnore,
    };

    m_filterModeMenu = ControlUtil::createMenu(m_menu);

    m_filterModeActions = new QActionGroup(m_filterModeMenu);

    int index = 0;
    for (const QString &name : FirewallConf::filterModeNames()) {
        if (Q_UNLIKELY(index >= std::size(filterModeIniKeys)))
            break;

        const char *iniKey = filterModeIniKeys[index];

        QAction *a = addAction(m_filterModeMenu,
                { /*iconPath=*/ {}, /*receiver=*/nullptr, /*member=*/nullptr, ActionNone,
                        /*checkable=*/true });
        a->setText(name);

        addHotKey(a, iniKey);

        // TODO: Implement Ask to Connect
        a->setEnabled(index != 1);

        m_filterModeActions->addAction(a);
        ++index;
    }

    connect(m_filterModeActions, &QActionGroup::triggered, this, &TrayIcon::switchFilterMode);
}

void TrayIcon::setupTrayMenuGroupActions()
{
    for (int i = 0; i < MAX_APP_GROUP_COUNT; ++i) {
        QAction *a = addAction(m_menu,
                { QString(), this, SLOT(switchTrayFlag(bool)), ActionNone, /*checkable=*/true });

        if (i < MAX_FKEY_COUNT) {
            addHotKey(a, HotKey::appGroupModifier);
        }

        m_appGroupActions.append(a);
    }
}

void TrayIcon::setupTrayMenuRuleActions()
{
    for (int i = 0; i < MAX_RULE_ACTIONS_COUNT; ++i) {
        QAction *a = addAction(m_menu,
                { QString(), this, SLOT(switchTrayRuleFlag(bool)), ActionNone,
                        /*checkable=*/true });

        if (i < MAX_FKEY_COUNT) {
            addHotKey(a, HotKey::ruleModifier);
        }

        m_ruleActions.append(a);
    }
}

void TrayIcon::setupTrayMenuBottomActions()
{
    m_quitAction = addAction(m_menu, { ":/icons/standby.png", this, SLOT(quitProgram()) });
    addHotKey(m_quitAction, HotKey::quit);

    m_trayMenuAction =
            addAction(m_menu, { QString(), this, SLOT(switchTrayMenu(bool)), ActionShowTrayMenu });
    m_trayMenuAction->setVisible(false);
}

void TrayIcon::updateTrayMenuFlags()
{
    const bool editEnabled =
            (!settings()->isPasswordRequired() && !windowManager()->isWindowOpen(WindowOptions));

    m_filterEnabledAction->setEnabled(editEnabled);
    m_filterEnabledAction->setChecked(conf()->filterEnabled());

    m_snoozeAlertsAction->setEnabled(editEnabled);
    m_snoozeAlertsAction->setChecked(iniUser()->progSnoozeAlerts());

    m_blockTrafficMenu->setEnabled(editEnabled);
    {
        const int index = conf()->blockTrafficIndex();
        QAction *action = m_blockTrafficActions->actions().at(index);
        if (!action->isChecked()) {
            action->setChecked(true);
            updateBlockTrafficMenuIcon(index);
        }
    }

    m_filterModeMenu->setEnabled(editEnabled);
    {
        const int index = conf()->filterMode();
        QAction *action = m_filterModeActions->actions().at(index);
        if (!action->isChecked()) {
            action->setChecked(true);
            updateFilterModeMenuIcon(index);
        }
    }

    int appGroupIndex = 0;
    for (QAction *action : std::as_const(m_appGroupActions)) {
        if (!action->isVisible())
            break;

        const bool appGroupEnabled = conf()->appGroupEnabled(appGroupIndex++);

        action->setEnabled(editEnabled);
        action->setChecked(appGroupEnabled);
    }
}

void TrayIcon::updateAppGroupActions()
{
    const int trayMaxGroups = iniUser()->trayMaxGroups(MAX_APP_GROUP_COUNT);
    const int appGroupsCount = qMin(conf()->appGroups().size(), trayMaxGroups);

    for (int i = 0; i < MAX_APP_GROUP_COUNT; ++i) {
        QAction *action = m_appGroupActions.at(i);

        const bool visible = (i < appGroupsCount);
        QString menuLabel;

        if (visible) {
            const AppGroup *appGroup = conf()->appGroups().at(i);
            menuLabel = appGroup->menuLabel();
        }

        action->setText(menuLabel);
        action->setVisible(visible);
        action->setEnabled(visible);
    }
}

void TrayIcon::updateRuleActions()
{
    const auto ruleIds = confRuleManager()->getRuleMenuIds();
    const int rulesCount = ruleIds.size();

    for (int i = 0; i < MAX_RULE_ACTIONS_COUNT; ++i) {
        QAction *action = m_ruleActions.at(i);

        const bool visible = (i < rulesCount);
        quint16 ruleId = 0;
        QString menuLabel;

        if (visible) {
            ruleId = ruleIds[i];
            menuLabel = confRuleManager()->ruleNameById(ruleId);
        }

        action->setText(menuLabel);
        action->setData(ruleId);
        action->setVisible(visible);
        action->setEnabled(visible);
    }
}

void TrayIcon::updateBlockTrafficMenuIcon(int index)
{
    const QStringList iconPaths = FirewallConf::blockTrafficIconPaths();
    const QString iconPath = iconPaths.at(index);

    m_blockTrafficMenu->setIcon(IconCache::icon(iconPath));
}

void TrayIcon::updateFilterModeMenuIcon(int index)
{
    const QStringList iconPaths = FirewallConf::filterModeIconPaths();
    const QString iconPath = iconPaths.at(index);

    m_filterModeMenu->setIcon(IconCache::icon(iconPath));
}

void TrayIcon::sendAlertMessage()
{
    if (!checkAlertFilterMode(conf(), iniUser()))
        return;

    if (iniUser()->progNotifyMessage()) {
        windowManager()->showTrayMessage(
                tr("New program detected!"), WindowManager::TrayMessageAlert);
    }

    if (iniUser()->progAlertSound()) {
        OsUtil::playSound();
    }

    if (iniUser()->progAlertWindowAutoShow() && !iniUser()->progSnoozeAlerts()) {
        windowManager()->showProgramAlertWindow(/*activate=*/false);
    }
}

void TrayIcon::updateAlertTimer()
{
    if (!iniUser()->trayAnimateAlert()) {
        removeAlertTimer();
        return;
    }

    setupAlertTimer();

    m_animatedAlert = m_alerted;

    if (m_alerted) {
        m_alertTimer->start();
    } else {
        m_alertTimer->stop();
    }
}

void TrayIcon::setupAlertTimer()
{
    if (m_alertTimer)
        return;

    m_alertTimer = new QTimer(this);
    m_alertTimer->setInterval(1000);

    connect(m_alertTimer, &QTimer::timeout, this, [&] {
        m_animatedAlert = !m_animatedAlert;
        updateTrayIconShape();
    });
}

void TrayIcon::removeAlertTimer()
{
    if (!m_alertTimer)
        return;

    delete m_alertTimer;
    m_alertTimer = nullptr;
}

void TrayIcon::updateTrayIconShape()
{
    bool isDefault = false;
    setIconPath(trayIconPath(isDefault));

    const QIcon icon = getTrayIcon();

    this->setIcon(icon);

    windowManager()->taskbarButton().setApplicationBadge(isDefault ? QIcon() : icon);
}

QIcon TrayIcon::getTrayIcon() const
{
    if (m_alerted) {
        const QString alertIconPath = ":/icons/error.png";

        return m_animatedAlert ? IconCache::icon(alertIconPath)
                               : GuiUtil::overlayIcon(iconPath(), alertIconPath);
    }

    return IconCache::icon(iconPath());
}

QString TrayIcon::trayIconPath(bool &isDefault) const
{
    if (!conf()->filterEnabled() || !driverManager()->isDeviceOpened()) {
        return ":/icons/fort_gray.png";
    }

    return trayIconBlockPath(conf()->blockTrafficIndex(), isDefault);
}

QString TrayIcon::trayIconBlockPath(int blockType, bool &isDefault) const
{
    switch (blockType) {
    case FirewallConf::BlockTrafficAll:
        return ":/icons/fort_deny.png";
    case FirewallConf::BlockTrafficInetLan:
        return ":/icons/fort_red.png";
    case FirewallConf::BlockTrafficInet:
    case FirewallConf::BlockTrafficLan:
        return ":/icons/fort_orange.png";
    default:
        isDefault = true;
        return ":/icons/fort.png";
    }
}

void TrayIcon::saveTrayFlags()
{
    conf()->setFilterEnabled(m_filterEnabledAction->isChecked());
    iniUser()->setProgSnoozeAlerts(m_snoozeAlertsAction->isChecked());

    // Set Block Traffic
    {
        QAction *action = m_blockTrafficActions->checkedAction();
        const int index = m_blockTrafficActions->actions().indexOf(action);
        if (conf()->blockTrafficIndex() != index) {
            conf()->setBlockTrafficIndex(index);
            updateBlockTrafficMenuIcon(index);
        }
    }

    // Set Filter Mode
    {
        QAction *action = m_filterModeActions->checkedAction();
        const int index = m_filterModeActions->actions().indexOf(action);
        if (conf()->filterMode() != index) {
            conf()->setFilterMode(FirewallConf::FilterMode(index));
            updateFilterModeMenuIcon(index);
        }
    }

    // Set App. Groups' enabled states
    int i = 0;
    for (AppGroup *appGroup : conf()->appGroups()) {
        const QAction *action = m_appGroupActions.at(i++);
        if (!action->isVisible())
            break;

        appGroup->setEnabled(action->isChecked());
    }

    confManager()->saveFlags();
}

void TrayIcon::onShowWindowAction()
{
    const auto action = qobject_cast<QAction *>(sender());
    Q_ASSERT(action);

    const auto windowCode = WindowCode(action->data().toInt());

    constexpr int alwaysSwitchWindows = WindowGraph;

    if ((windowCode & alwaysSwitchWindows) != 0 || iniUser()->traySwitchWindow()) {
        windowManager()->switchWindowByCode(windowCode);
    } else {
        windowManager()->showWindowByCode(windowCode);
    }
}

void TrayIcon::switchTrayFlag(bool checked)
{
    if (iniUser()->confirmTrayFlags()) {
        const auto action = qobject_cast<QAction *>(sender());
        Q_ASSERT(action);

        windowManager()->showQuestionBox(
                [=, this](bool confirmed) {
                    if (confirmed) {
                        saveTrayFlags();
                    } else {
                        action->setChecked(!checked);
                    }
                },
                tr("Are you sure to switch the \"%1\"?").arg(action->text()));
    } else {
        saveTrayFlags();
    }
}

void TrayIcon::switchBlockTraffic(QAction *action)
{
    const int index = m_blockTrafficActions->actions().indexOf(action);
    if (index < 0 || index == conf()->blockTrafficIndex())
        return;

    if (iniUser()->confirmTrayFlags()) {
        windowManager()->showQuestionBox(
                [=, this](bool confirmed) {
                    if (confirmed) {
                        saveTrayFlags();
                    } else {
                        QAction *a =
                                m_blockTrafficActions->actions().at(conf()->blockTrafficIndex());
                        a->setChecked(true);
                    }
                },
                tr("Are you sure to select the \"%1\"?").arg(action->text()));
    } else {
        saveTrayFlags();
    }
}

void TrayIcon::switchFilterMode(QAction *action)
{
    const int index = m_filterModeActions->actions().indexOf(action);
    if (index < 0 || index == conf()->filterMode())
        return;

    if (iniUser()->confirmTrayFlags()) {
        windowManager()->showQuestionBox(
                [=, this](bool confirmed) {
                    if (confirmed) {
                        saveTrayFlags();
                    } else {
                        QAction *a = m_filterModeActions->actions().at(conf()->filterMode());
                        a->setChecked(true);
                    }
                },
                tr("Are you sure to select the \"%1\"?").arg(action->text()));
    } else {
        saveTrayFlags();
    }
}

void TrayIcon::switchTrayRuleFlag(bool checked)
{
    const auto action = qobject_cast<QAction *>(sender());

    const quint16 ruleId = action->data().toUInt();

    confRuleManager()->updateRuleEnabled(ruleId, checked);
}

void TrayIcon::updateActionHotKeys()
{
    int groupIndex = 0;
    int ruleIndex = 0;
    int index = 0;
    for (auto action : hotKeyManager()->actions()) {
        const auto &iniKey = m_actionIniKeys[index];

        const QString shortcutText = iniUser()->hotKeyValue(iniKey);
        QKeySequence shortcut = QKeySequence::fromString(shortcutText);

        if (!shortcut.isEmpty() && iniKey == HotKey::appGroupModifier) {
            const QKeyCombination key = shortcut[0];

            shortcut = key.keyboardModifiers() | (Qt::Key_F1 + groupIndex);
            ++groupIndex;
        }

        if (!shortcut.isEmpty() && iniKey == HotKey::ruleModifier) {
            const QKeyCombination key = shortcut[0];

            shortcut = key.keyboardModifiers() | (Qt::Key_F1 + ruleIndex);
            ++ruleIndex;
        }

        action->setShortcut(shortcut);

        ++index;
    }
}

void TrayIcon::addHotKey(QAction *action, const char *iniKey)
{
    hotKeyManager()->addAction(action);

    m_actionIniKeys.append(iniKey);
}

void TrayIcon::updateHotKeys()
{
    hotKeyManager()->initialize(iniUser()->hotKeyEnabled(), iniUser()->hotKeyGlobal());
}

TrayIcon::ActionType TrayIcon::clickEventActionType(IniUser *iniUser, ClickType clickType)
{
    const QString eventName = clickNameByType(clickType);
    const QString actionName = iniUser->trayAction(eventName);

    const ActionType actionType = actionTypeByName(actionName);
    return (actionType != ActionNone) ? actionType : defaultActionTypeByClick(clickType);
}

void TrayIcon::setClickEventActionType(IniUser *iniUser, ClickType clickType, ActionType actionType)
{
    const QString eventName = clickNameByType(clickType);
    const QString actionName = actionNameByType(actionType);

    iniUser->setTrayAction(eventName, actionName);
}

void TrayIcon::resetClickEventActionType(IniUser *iniUser, ClickType clickType)
{
    const TrayIcon::ActionType actionType = defaultActionTypeByClick(clickType);

    setClickEventActionType(iniUser, clickType, actionType);
}

void TrayIcon::updateClickActions()
{
    for (int i = 0; i < ClickTypeCount; ++i) {
        m_clickActions[i] = clickActionFromIni(ClickType(i));
    }
}

QAction *TrayIcon::clickAction(TrayIcon::ClickType clickType) const
{
    return m_clickActions[clickType];
}

QAction *TrayIcon::clickActionFromIni(TrayIcon::ClickType clickType) const
{
    const ActionType actionType = clickEventActionType(iniUser(), clickType);

    return clickActionByType(actionType);
}

QAction *TrayIcon::clickActionByType(TrayIcon::ActionType actionType) const
{
    QAction *const actions[] = {
        m_homeAction,
        m_programsAction,
        m_programsOrAlertAction,
        m_optionsAction,
        m_statisticsAction,
        m_graphAction,
        m_filterEnabledAction,
        m_snoozeAlertsAction,
        m_blockTrafficMenuAction,
        m_filterModeMenuAction,
        m_trayMenuAction,
    };

    if (actionType > TrayIcon::ActionNone && actionType < TrayIcon::ActionIgnore) {
        return actions[actionType];
    }

    return nullptr;
}

void TrayIcon::processMouseClick(Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    const auto clickType = clickTypeByMouse(button, modifiers);

    onMouseClicked(clickType, /*menuClickType=*/SingleClick);
}

void TrayIcon::onMouseClicked(TrayIcon::ClickType clickType, TrayIcon::ClickType menuClickType)
{
    QAction *action = clickAction(clickType);

    if (action) {
        if (clickType == menuClickType && action->data().toInt() == TrayIcon::ActionShowTrayMenu) {
            return; // already handled by context-menu logic
        }

        QMetaObject::invokeMethod(action, &QAction::trigger, Qt::QueuedConnection);
    }

    if (clickType == menuClickType) {
        // revert the default action: close context-menu
        hideTrayMenuLater();
    }
}

void TrayIcon::onTrayActivatedByTrigger()
{
    const Qt::KeyboardModifiers modifiers = QApplication::queryKeyboardModifiers();
    const ClickType clickType = (modifiers & Qt::ControlModifier) != 0
            ? CtrlSingleClick
            : ((modifiers & Qt::AltModifier) != 0 ? AltSingleClick : SingleClick);

    if (clickAction(DoubleClick)) {
        m_trayTriggered = true;
        QTimer::singleShot(QApplication::doubleClickInterval(), this, [=, this] {
            if (m_trayTriggered) {
                onTrayActivatedByClick(clickType);
            }
        });
    } else {
        onTrayActivatedByClick(clickType);
    }
}

void TrayIcon::onTrayActivatedByClick(TrayIcon::ClickType clickType)
{
    m_trayTriggered = false;

    onMouseClicked(clickType);
}

void TrayIcon::showProgramsOrAlertWindow()
{
    if (m_alerted) {
        windowManager()->showProgramAlertWindow();
    } else {
        m_programsAction->trigger();
    }
}

void TrayIcon::onWindowVisibilityChanged(WindowCode code, bool isVisible)
{
    switch (code) {
    case WindowPrograms:
    case WindowProgramAlert: {
        updateTrayIcon(/*alerted=*/false);
    } break;
    case WindowOptions: {
        updateTrayMenuFlags();
    } break;
    case WindowGraph: {
        m_graphAction->setChecked(isVisible);
    } break;
    }
}
