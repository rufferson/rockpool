#ifndef PLATFORMINTERFACE_H
#define PLATFORMINTERFACE_H

#include "libpebble/pebble.h"
#include "libpebble/musicmetadata.h"

#include <QObject>
#include <QJsonObject>

static const QUuid uuid_ns_dns("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
class PlatformInterface: public QObject
{
    Q_OBJECT
public:
    PlatformInterface(QObject *parent = 0): QObject(parent) {}
    virtual ~PlatformInterface() {}
    // App Specific Resources for Pins. Initialized in core.cpp
    // type: {icon, color, [mute_name, [...]]}
    static const QHash<QString,QStringList> AppResMap;
    // UUID_NOTIFICATIONS_DATA_SOURCE - allows built-in actions to be relayed back to system, eg. dismiss
    const static QString SysID;
    const static QUuid UUID;
    static inline QUuid idToGuid(QString id) {
        return QUuid::createUuidV5(uuid_ns_dns,QString("%1.pin.rockpool.nemomobile.org").arg(id));
    }

// Device state
public:
    virtual bool deviceIsActive() const = 0;
    virtual void setProfile(const QString &profile) const = 0;
signals:
    void deviceActiveChanged();
    void timeChanged();

// Notifications
public:
    virtual void actionTriggered(const QUuid &uuid, const QString &actToken, const QJsonObject &param) const = 0;
    virtual void removeNotification(const QUuid &uuid) const = 0;
    virtual const QHash<QString,QStringList>& cannedResponses() const = 0;
    virtual void setCannedResponses(const QHash<QString,QStringList> &cans) = 0;
    virtual void sendTextMessage(const QString &account, const QString &contact, const QString &text) const = 0;
signals:
    void newTimelinePin(const QJsonObject &pin);

// Music
public:
    virtual void sendMusicControlCommand(MusicControlButton controlButton) = 0;
    virtual MusicMetaData musicMetaData() const = 0;
    virtual MusicPlayState getMusicPlayState() const = 0;

signals:
    void musicPlayStateChanged(const MusicPlayState &playState);
    void musicMetadataChanged(MusicMetaData metaData);

// Phone calls
signals:
    void incomingCall(uint cookie, const QString &number, const QString &name);
    void callStarted(uint cookie);
    void callEnded(uint cookie, bool missed);
public:
    virtual void hangupCall(uint cookie) = 0;

// Organizer
public:
    virtual void syncOrganizer(qint32 end) const = 0;
    virtual void stopOrganizer() const = 0;
signals:
    void delTimelinePin(const QString &guid);

// Dictation
public:
    virtual void voiceSessionRequest(const QBluetoothAddress &pblAddr, quint16 sid, const QUuid &appUuid, const SpeexInfo &codec) = 0;
    virtual void voiceAudioStream(const QBluetoothAddress &pblAddr, quint16 sid, const AudioStream &frames) = 0;
    virtual void voiceSessionClose(const QBluetoothAddress &pblAddr, quint16 sid) = 0;
signals:
    void voiceSessionResponse(const QBluetoothAddress &addr, quint16 sesId, quint8 result);
    void voiceSessionResult(const QBluetoothAddress &addr, quint16 sesId, const QVariantList &sentences);
    void voiceAudioStop(const QBluetoothAddress &addr, quint16 sesId);

};

#endif // PLATFORMINTERFACE_H
