#ifndef SERVICEMANAGERIFACE_H
#define SERVICEMANAGERIFACE_H

#include <QObject>

class ServiceManagerIface
{
public:
    ServiceManagerIface() = default;
    virtual ~ServiceManagerIface() = default;

    virtual bool acceptStop() const { return true; }
    virtual bool acceptPauseContinue() const { return true; }

    virtual void initialize(qintptr hstatus);

    void registerDeviceNotification();
    void unregisterDeviceNotification();

    virtual const wchar_t *serviceName() const = 0;

    virtual void processControl(quint32 code, quint32 eventType) = 0;

    static bool isDeviceEvent(quint32 eventType);
    static bool isPowerEvent(quint32 eventType);

protected:
    void setupAcceptedControls();

    static void reportStatus(quint32 code = 0);
};

#endif // SERVICEMANAGERIFACE_H
