#include "dictationhandler.h"

#include "libpebble/voiceendpoint.h"
#include "libpebble/watchdatawriter.h"

#include <speex/speex.h>

#include <QDebug>
#include <QTemporaryFile>

DictationHandler::DictationHandler(QObject *parent, const QBluetoothAddress &addr) :
    QObject(parent),
    m_address(addr)
{
    qDebug() << "Created dictation service for " << addr.toString();
}

qint64 ogg_write_page(ogg_page *op, QIODevice *d) {
    return d->write((const char *)op->header, op->header_len) + d->write((const char *)op->body, op->body_len);
}
qint64 ogg_flush_stream(ogg_stream_state *os, QIODevice *d) {
    qint64 ret = 0;
    ogg_page og;
    while(ogg_stream_flush(os,&og)) {
        qint64 len = ogg_write_page(&og,d);
        if(len != og.header_len + og.body_len) {
            qCritical() << "Failure writing ogg page: (" << len << "<>" << (og.body_len + og.header_len) << ")" << d->errorString();
            return 0;
        }
        ret += len;
    }
    return ret;
}

void DictationHandler::voiceSessionRequest(const QUuid &appUuid, quint16 sesId, const SpeexInfo &codec)
{
    if(m_voiceSessDump) {
        emit voiceSessionResponse(m_address, sesId, VoiceEndpoint::ResInvalidMessage);
        return;
    }
    m_voiceSessDump = new QTemporaryFile(this);
    if(m_voiceSessDump->open()) {
        m_sessId = sesId;
        m_appUid = appUuid;
        /*
        QString mime = QString("audio/speex; rate=%1; bitrate=%2; bitstreamver=%3; frame=%4").arg(
                QString::number(codec.sampleRate),
                QString::number(codec.bitRate),
                QString::number(codec.bitstreamVer),
                QString::number(codec.frameSize));
        emit parent()->parent()->voiceSessionSetup(m_address.toString(),m_voiceSessDump->fileName(),mime,appUuid.toString());
        */
        /**
         * Since we're storing the stream - we need to put it into container (as opposite to rtp streaming)
         * Standard container for speex is ogg. So let's start packing stuff.
         */
        if(!ogg_stream_init(&os,qrand())) {
            int op_len;
            speex_init_header(&sph,codec.sampleRate,1,&speex_wb_mode);
            sph.frames_per_packet = 1;
            sph.bitrate = codec.bitRate;
            sph.frame_size = codec.frameSize;
            sph.mode_bitstream_version = codec.bitstreamVer;
            //
            op.packet = (unsigned char *)speex_header_to_packet(&sph,&op_len);
            op.bytes = op_len;
            op.b_o_s = 1;
            op.e_o_s = 0;
            op.granulepos = 0;
            op.packetno = 0;
            ogg_stream_packetin(&os,&op);
            free(op.packet);
            if(ogg_flush_stream(&os,m_voiceSessDump) > 0) {
                QString com="Wrapped by Rockpool %1 from %2 for speex %3";
                QByteArray com_pkt;
                QByteArray comment = com.arg(QStringLiteral(VERSION)).arg(m_address.toString()).arg(QString::fromUtf8(codec.version)).toLocal8Bit();
                WatchDataWriter w(&com_pkt);
                w.writeLE<quint32>(comment.length());
                w.writeBytes(comment.length(),comment);
                w.writeLE<quint32>(0); // av-pair section length
                op.packet = (unsigned char*)com_pkt.constData();
                op.bytes = com_pkt.length();
                op.b_o_s = 0;
                op.packetno++;
                ogg_stream_packetin(&os,&op);
                // Inform requestor of our readiness
                if(ogg_flush_stream(&os,m_voiceSessDump) > 0) {
                    emit voiceSessionResponse(m_address, m_sessId, VoiceEndpoint::ResSuccess);
                    qDebug() << "Opened session" << m_sessId << " for (" << comment << ") to" << m_voiceSessDump->fileName() << "from" << appUuid.toString();
                    return;
                }
                qCritical() << "Could not write OGG header to " << m_voiceSessDump->fileName();
            } else
                qCritical() << "Could not flush OGG BOS header to" << m_voiceSessDump->fileName() << m_voiceSessDump->errorString();
        } else
            qCritical() << "Ogg stream initialisation failed";
    }
    m_voiceSessDump->deleteLater();
    emit voiceSessionResponse(m_address,sesId,VoiceEndpoint::ResServiceUnavail);
    m_voiceSessDump = nullptr;
}
/*
    emit voiceSessionClosed(m_voiceSessDump->fileName());
    m_voiceSessDump->deleteLater();
    m_voiceSessDump = 0;
*/
void DictationHandler::voiceAudioStream(quint16 sesId, const AudioStream &frames)
{
    if(sesId != m_sessId) {
        qWarning() << "Received stream for unknown session" << sesId << "- aborting the sequence";
        // Abort received session
        emit voiceAudioStop(m_address,sesId);
        emit voiceSessionResponse(m_address,sesId,VoiceEndpoint::ResRecognizerError);
        // also existing session
        emit voiceAudioStop(m_address,m_sessId);
        emit voiceSessionResponse(m_address,m_sessId,VoiceEndpoint::ResRecognizerError);
    }
    if(frames.count>0) {
        int i = 0;
        if(op.packetno == 1) {
            //emit voiceSessionStream(m_voiceSessDump->fileName());
            qDebug() << "Audio Stream has started dumping" << frames.count << "frames to" << m_voiceSessDump->fileName();
            m_buf = frames.frames.at(0).data;
            op.packet = (unsigned char*)m_buf.constData();
            op.bytes = m_buf.length();
            op.granulepos = (op.packetno)*sph.frame_size;
            op.packetno++;
            i++;
        }
        for(;i<frames.count;i++) {
            ogg_stream_packetin(&os,&op);
            if(ogg_flush_stream(&os,m_voiceSessDump)==0) {
                qCritical() << "Cannot write frames to" << m_voiceSessDump->fileName() << m_voiceSessDump->errorString();
                emit voiceAudioStop(m_address,m_sessId);
                emit voiceSessionResponse(m_address,m_sessId,VoiceEndpoint::ResRecognizerError);
                return;
            }
            m_buf = frames.frames.at(0).data;
            op.packet = (unsigned char*)m_buf.constData();
            op.bytes = m_buf.length();
            op.granulepos = (op.packetno)*sph.frame_size;
            op.packetno++;
        }
    } else {
        op.e_o_s = 1;
        ogg_stream_packetin(&os,&op);
        if(ogg_flush_stream(&os,m_voiceSessDump)==0) {
            qCritical() << "Cannot write frames to" << m_voiceSessDump->fileName() << m_voiceSessDump->errorString();
            emit voiceSessionResponse(m_address,m_sessId,VoiceEndpoint::ResRecognizerError);
            return;
        }
        qDebug() << "Audio Stream has finished dumping" << op.packetno << "packets and" << op.granulepos << "samples to" << m_voiceSessDump->fileName();
        m_voiceSessDump->close();
        ogg_stream_clear(&os);
        m_voiceSessDump->copy("/home/nemo/pebble.ogg");
        //emit voiceSessionDumped(m_voiceSessDump->fileName());
    }
}

void DictationHandler::voiceSessionClose(quint16 sesId)
{
    if(sesId == m_sessId) {
        if(m_voiceSessDump->isOpen()) {
            qDebug() << "Audio transfer didn't happen or was aborted, cleaning up" << m_voiceSessDump->fileName();
            m_voiceSessDump->close();
            ogg_stream_clear(&os);
        }
        m_voiceSessDump->deleteLater();
        m_voiceSessDump = nullptr;
        m_sessId = -1;
    } else
        qWarning() << "Ignoring closure notification for unknown session" << sesId << m_sessId;
}
