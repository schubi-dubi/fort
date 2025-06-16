#include "appinforow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

#include <appinfo/appinfocache.h>
#include <appinfo/appinfoutil.h>
#include <util/guiutil.h>

#include "controlutil.h"

AppInfoRow::AppInfoRow(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void AppInfoRow::retranslateUi()
{
    m_btAppCopyPath->setToolTip(tr("Copy Path"));
    m_btAppOpenFolder->setToolTip(tr("Open Folder"));
}

void AppInfoRow::setupUi()
{
    m_btAppCopyPath = ControlUtil::createIconToolButton(":/icons/page_copy.png");
    m_btAppOpenFolder = ControlUtil::createIconToolButton(":/icons/folder.png");

    m_lineAppPath = ControlUtil::createLineLabel();

    m_labelAppProductName = ControlUtil::createLabel();
    m_labelAppProductName->setFont(GuiUtil::fontBold());

    m_labelAppCompanyName = ControlUtil::createLabel();

    connect(m_btAppCopyPath, &QAbstractButton::clicked, this, &AppInfoRow::appCopyPath);
    connect(m_btAppOpenFolder, &QAbstractButton::clicked, this, &AppInfoRow::appOpenFolder);

    auto layout = ControlUtil::createHLayout();
    layout->addWidget(m_btAppCopyPath);
    layout->addWidget(m_btAppOpenFolder);
    layout->addWidget(m_lineAppPath, 1);
    layout->addWidget(m_labelAppProductName);
    layout->addWidget(m_labelAppCompanyName);

    setLayout(layout);
}

void AppInfoRow::refreshAppInfoVersion(const QString &appPath, AppInfoCache *appInfoCache)
{
    const auto appInfo = appInfoCache->appInfo(appPath);

    m_filePath = appInfo.filePath(appPath);

    m_lineAppPath->setText(appPath);

    m_labelAppProductName->setVisible(!appInfo.productName.isEmpty());
    m_labelAppProductName->setText(appInfo.productName + ' ' + appInfo.productVersion);

    m_labelAppCompanyName->setVisible(!appInfo.companyName.isEmpty());
    m_labelAppCompanyName->setText(appInfo.companyName);
}

void AppInfoRow::appCopyPath()
{
    GuiUtil::setClipboardData(m_lineAppPath->text());
}

void AppInfoRow::appOpenFolder()
{
    AppInfoUtil::openFolder(m_filePath);
}
