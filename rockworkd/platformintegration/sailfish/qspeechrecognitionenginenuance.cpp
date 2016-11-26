#include "qspeechrecognitionenginenuance.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QFile>

QStringList QSpeechRecognitionEngineNuance::supportedParameters = {
    "Locale",
    "Dictionary",       // Dictation, WebSearch, DTV-Search
    "AudioInputFile",   // A voice dump file
    "AudioInputDevice", // SpeakerAndMicrophone, HeadsetInOut, HeadsetBT, HeadPhone, LineOut
    "AudioSampleRate",  // '', 8kHz, 16kHz
    "AudioResolution",  // '', 16bit
    "AudioFormat",      // x-wav, x-speex, amr, qcelp, evrc
    "AudioCodec",       // '', pcm
    "AdaptationId",     // Anonimized User ID (eg. PDA MAC) for SD-AMA profile reference
    "ApplicationId",    // Nuance Application ID (NMAID)
    "ApplicationKey"    // Nuance Customer Key (credentials)
};
QString QSpeechRecognitionEngineNuance::url_template = "https://dictation.nuancemobility.net/NMDPAsrCmdServlet/dictation?appId=%1&appKey=%2&id=%3";

QSpeechRecognitionEngineNuance::QSpeechRecognitionEngineNuance(const QString &name, const QVariantMap &params, QObject *parent) :
    QObject(parent),
    m_nam(nullptr),
    m_name(name),
    m_params(params)
{
    qDebug() << "Creating Nuance HTTP QSpeechEngine";
}
QSpeechRecognitionEngineNuance::~QSpeechRecognitionEngineNuance()
{
    qDebug() << "Destroying Nuance HTTP QSpeechEngine";
}

void QSpeechRecognitionEngineNuance::setNAM(QNetworkAccessManager *nam)
{
    m_nam = nam;
}

QSpeechRecognitionEngineNuance::Error QSpeechRecognitionEngineNuance::updateParameter(const QString &key, const QVariant &value, QString *errStr)
{
    if(QSpeechRecognitionEngineNuance::supportedParameters.contains(key)) {
        if(value.isNull())
            m_params.remove(key);
        else
            m_params.insert(key,value);
        return ErrSuccess;
    }
    if(errStr)
        *errStr = QString("Unrecognized parameter: %1").arg(key);
    return ErrInvalidMessage;
}

QString QSpeechRecognitionEngineNuance::mimeType()
{
    QString ret;
    if(m_params.contains("AudioFormat")) {
        ret = QString("audio/%1").arg(m_params.value("AudioFormat").toString());
        foreach(const QString param, QStringList({"AudioCodec","AudioResolution","AudioSampleRate"})) {
            if(m_params.contains(param))
                ret += QString(";%1").arg(m_params.value(param).toString());
        }
    }
    return ret;
}

bool QSpeechRecognitionEngineNuance::buildNetworkRequest(QNetworkRequest &req)
{
    if(!m_params.contains("ApplicationId") || !m_params.contains("ApplicationKey") || !m_params.contains("AdaptationId"))
        return false;
    QUrl url = QUrl(url_template.arg(m_params.value("ApplicationId").toString(),m_params.value("ApplicationKey").toString(),m_params.value("AdaptationId").toString()));
    req=QNetworkRequest(QUrl(url));
    QString contentType = mimeType();
    if(!contentType.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader,contentType);
    if(m_params.contains("Locale"))
        req.setRawHeader("Accept-Language",m_params.value("Locale").toString().toLatin1());
    if(m_params.contains("Dictionary"))
        req.setRawHeader("Accept-Topic",m_params.value("Dictionary").toString().toLatin1());
    if(m_params.contains("AudioInputDevice"))
        req.setRawHeader("X-Dictation-AudioSource",m_params.value("AudioInputDevice").toString().toLatin1());
    if(!m_params.value("AudioInputFile").toString().isEmpty()) {
        QFile f(m_params.value("AudioInputFile").toString());
        if(f.open(QIODevice::ReadOnly)) {
            req.setHeader(QNetworkRequest::ContentLengthHeader,f.size());
            f.close();
        } else
            return false;
    } else
        req.setRawHeader("Transfer-Encoding","chunked");
    req.setRawHeader("X-Dictation-NBestListSize","1");
    req.setRawHeader("Accept","text/plain");
    return true;
}

/**
 * @brief QSpeechRecognitionEngineNuance::startListening
 * @param session - internal dictation session id
 * @param mute - when set expects delayed upload - which is file capture and send.
 * @param errStr - pointer to error detail buffer
 * @return
 */
QSpeechRecognitionEngineNuance::Error QSpeechRecognitionEngineNuance::startListening(int session, bool mute, QString *errStr)
{
    if(!mute) {
        if(errStr)
            *errStr = "Qt NAM does not support chunked post (hence streaming mode)";
        else
            qWarning() << "Qt NAM does not support chunked post (hence streaming mode)";
        return ErrServiceUnavail;
    }
    m_session = session;
    return ErrSuccess;
}

void QSpeechRecognitionEngineNuance::unmute(qint64 timestamp)
{
    Q_UNUSED(timestamp);
    qWarning() << "Qt NAM does not support chunked post (hence streaming mode)";
}

void QSpeechRecognitionEngineNuance::stopListening(qint64 timestamp)
{
    QNetworkRequest req;
    Q_UNUSED(timestamp);
    if(buildNetworkRequest(req)) {
        QFile f(m_params.value("AudioInputFile").toString());
        if(f.open(QIODevice::ReadOnly)) {
            QNetworkReply *rpl = m_nam->post(req,&f);
            connect(rpl, &QNetworkReply::finished, [this,rpl](){
                rpl->deleteLater();
                if(rpl->error()==QNetworkReply::NoError && rpl->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("text/plain")) {
                    QString text = QString::fromUtf8(rpl->readAll());
                    QVariantMap ret;
                    ret.insert("Transcription",text);
                    emit result(m_session,nullptr,ret);
                    m_session = -1;
                }
            });
            f.close();
        }
    }
}

bool QSpeechRecognitionEngineNuance::process()
{
    qDebug() << "Streaming is not supported thus far, don't even try that";
    return false;
}
QSpeechRecognitionGrammar * QSpeechRecognitionEngineNuance::createGrammar(const QString &name, const QUrl &location, QString *errStr)
{
    if(errStr)
        *errStr = QString("Don't know how to create grammar %1 at %2").arg(name,location.toString());
    return nullptr;
}

QSpeechRecognitionEngineNuance::Error QSpeechRecognitionEngineNuance::setGrammar(QSpeechRecognitionGrammar *grammar, QString *errStr)
{
    Q_UNUSED(grammar);
    if(errStr)
        *errStr = "No idea how to set grammar";
    return ErrInvalidMessage;
}
void QSpeechRecognitionEngineNuance::resetAdaptationState()
{
    qDebug() << "Not implemented";
}
void QSpeechRecognitionEngineNuance::abortListening()
{
    qDebug() << "We're not streaming but let's clear the file" << m_params.value("AudioInputFile");
    m_params.remove("AudioInputFile");
    m_session = -1;
    emit attributeUpdated(m_session, "AudioInputFile", QString());
}
void QSpeechRecognitionEngineNuance::reset()
{
    abortListening();
}
