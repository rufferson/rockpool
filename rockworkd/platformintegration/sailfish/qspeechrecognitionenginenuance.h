#ifndef QSPEECHRECOGNITIONENGINENUANCE_H
#define QSPEECHRECOGNITIONENGINENUANCE_H

#include <QObject>
#include <QVariantMap>

class QNetworkRequest;
class QNetworkAccessManager;

struct QSpeechRecognitionGrammar {
    QString name;
};

class QSpeechRecognitionEngineNuance : public QObject
{
    Q_OBJECT
public:
    static QStringList supportedParameters;
    QSpeechRecognitionEngineNuance(const QString &name, const QVariantMap &params, QObject *parent);
    virtual ~QSpeechRecognitionEngineNuance();
    enum Error {
        ErrSuccess,
        ErrServiceUnavail,
        ErrTimeout,
        ErrServiceError,
        ErrServerError,
        ErrDisabled,
        ErrInvalidMessage
    };
    void setNAM(QNetworkAccessManager *nam);
    void setParameter(const QString &key, const QVariant &value, QString *errStr) {updateParameter(key,value,errStr);}

signals:
    void requestProcess();
    void requestStop(int session, qint64 timestamp);
    void result(int session, const QSpeechRecognitionGrammar *grammar, const QVariantMap &data);
    void error(int session, Error error, const QVariantMap &patams);
    void attributeUpdated(int session, const QString &key, const QVariant &value);

public slots:
    QSpeechRecognitionGrammar * createGrammar(const QString &name, const QUrl &location, QString *errStr);
    Error setGrammar(QSpeechRecognitionGrammar *grammar, QString *errStr);
    Error startListening(int session, bool mute, QString *errStr);
    void stopListening(qint64 timestamp);
    void unmute(qint64 timestamp);
    void abortListening();
    void reset();
    void resetAdaptationState();
    bool process();

protected:
    Error updateParameter(const QString &key, const QVariant &value, QString *errStr);
    QString mimeType();

private:
    static QString url_template;

    bool buildNetworkRequest(QNetworkRequest &req);
    QNetworkAccessManager *m_nam;

    QString m_name;
    QVariantMap m_params;

    int m_session = -1;
};

#endif // QSPEECHRECOGNITIONENGINENUANCE_H
