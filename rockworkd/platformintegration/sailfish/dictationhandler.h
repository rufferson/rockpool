#ifndef DICTATIONHANDLER_H
#define DICTATIONHANDLER_H

#include <ogg/ogg.h>
#include <speex/speex_header.h>

#include <QObject>
#include <QUuid>
#include <QBluetoothAddress>

class QTemporaryFile;
struct SpeexInfo;
struct AudioStream;

class DictationHandler : public QObject
{
    Q_OBJECT
public:
    explicit DictationHandler(QObject *parent, const QBluetoothAddress &addr);

signals:
    void voiceSessionResponse(const QBluetoothAddress &addr, quint16 sesId, quint8 result);
    void voiceSessionResult(const QBluetoothAddress &addr, quint16 sesId, const QVariantList &sentences);
    void voiceAudioStop(const QBluetoothAddress &addr, quint16 sesId);

public slots:
    void voiceSessionRequest(const QUuid &appUuid, quint16 sesId, const SpeexInfo &codec);
    void voiceAudioStream(quint16 sesId, const AudioStream &frames);
    void voiceSessionClose(quint16 sesId);

private:
    QBluetoothAddress m_address;

    qint32 m_sessId;
    QUuid m_appUid;
    QTemporaryFile* m_voiceSessDump = nullptr;
    ogg_stream_state os;
    ogg_packet op;
    SpeexHeader sph;
    QByteArray m_buf;
};

#endif // DICTATIONHANDLER_H
