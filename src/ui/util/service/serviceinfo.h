#ifndef SERVICEINFO_H
#define SERVICEINFO_H

#include <QObject>

class ServiceInfo
{
public:
    enum TrackFlag { TrackImagePath = 0x01, TrackType = 0x02 };

    enum Type : quint16 {
        TypeUnknown = 0,
        TypeWin32OwnProcess = 0x10, // SERVICE_WIN32_OWN_PROCESS
        TypeWin32ShareProcess = 0x20, // SERVICE_WIN32_SHARE_PROCESS
        TypeWin32 = (TypeWin32OwnProcess | TypeWin32ShareProcess), // SERVICE_WIN32
    };

    enum State {
        StateActive = 0x01, // SERVICE_ACTIVE
        StateInactive = 0x02, // SERVICE_INACTIVE
        StateAll = (StateActive | StateInactive), // SERVICE_STATE_ALL
    };

    bool isTracked() const { return trackFlags != 0; }
    bool isOwnProcess() const { return serviceType == ServiceInfo::TypeWin32OwnProcess; }

public:
    bool hasProcess : 1 = false;
    bool isHostSplitDisabled : 1 = false;
    Type serviceType = TypeUnknown;
    quint16 trackFlags = 0;
    quint32 processId = 0;
    QString serviceName;
    QString realServiceName;
    QString displayName;
};

#endif // SERVICEINFO_H
