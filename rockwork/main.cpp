#include <QtQuick>

#include <sailfishapp.h>

#include "notificationsourcemodel.h"
#include "servicecontrol.h"
#include "pebbles.h"
#include "pebble.h"
#include "applicationsmodel.h"
#include "applicationsfiltermodel.h"
#include "appstoreclient.h"
#include "screenshotmodel.h"

#include <qmozcontext.h>
#include <qmozenginesettings.h>
#define DEFAULT_COMPONENTS_PATH "/usr/lib/mozembedlite/"
#include <QTimer>

#ifndef ROCKPOOL_DATA_PATH
#define ROCKPOOL_DATA_PATH "/usr/share/rockpool/"
#endif //ROCKPOOL_DATA_PATH

int main(int argc, char *argv[])
{
    QScopedPointer<const QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setApplicationName("rockpool");
    app->setOrganizationName("");

    QSettings ini(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+"/"+app->applicationName()+"/app.ini",QSettings::IniFormat);
    QString locale = ini.contains("LANG") ? ini.value("LANG").toString() : QLocale::system().name();
    QTranslator i18n;
    i18n.load("rockpool_"+locale,QString(ROCKPOOL_DATA_PATH)+QString("translations"));
    app->installTranslator(&i18n);

    qmlRegisterUncreatableType<Pebble>("RockPool", 1, 0, "Pebble", "Get them from the model");
    qmlRegisterUncreatableType<ApplicationsModel>("RockPool", 1, 0, "ApplicationsModel", "Get them from a Pebble object");
    qmlRegisterUncreatableType<AppItem>("RockPool", 1, 0, "AppItem", "Get them from an ApplicationsModel");
    qmlRegisterType<ApplicationsFilterModel>("RockPool", 1, 0, "ApplicationsFilterModel");
    qmlRegisterType<Pebbles>("RockPool", 1, 0, "Pebbles");
    qmlRegisterUncreatableType<NotificationSourceModel>("RockPool", 1, 0, "NotificationSourceModel", "Get it from a Pebble object");
    qmlRegisterType<ServiceControl>("RockPool", 1, 0, "ServiceController");
    qmlRegisterType<AppStoreClient>("RockPool", 1, 0, "AppStoreClient");
    qmlRegisterType<ScreenshotModel>("RockPool", 1, 0, "ScreenshotModel");

    // Workaround for https://bugzilla.mozilla.org/show_bug.cgi?id=929879
    setenv("LC_NUMERIC", "C", 1);
    setlocale(LC_NUMERIC, "C");
    // This must be set or app segfaults under maplaunchd's boostable invoker
    setenv("GRE_HOME", app->applicationDirPath().toLocal8Bit().constData(), 1);
    setenv("USE_ASYNC", "1", 1);// Use Qt Assisted event loop instead of native full thread
    setenv("MP_UA", "1", 1);    // Use generic MobilePhone UserAgent string for Gecko

    QString componentPath(DEFAULT_COMPONENTS_PATH);
    QString cachePath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    QMozContext::GetInstance()->setProfile(cachePath);
    // Polite way would to emit startupcache-invalidate message to Context, but who cares
    QFileInfo manifest(QString(ROCKPOOL_DATA_PATH) + QString("jsm/RockpoolJSComponents.manifest"));
    QFileInfo cache(cachePath + "/.mozilla/startupCache/startupCache.4.little");
    qDebug() << "Manifest" << manifest.absoluteFilePath() << "modified" << manifest.lastModified().toString();
    qDebug() << "Cache" << cache.absoluteFilePath() << "modified" << cache.lastModified().toString();
    if(manifest.exists() && cache.exists() && manifest.lastModified() > cache.lastModified()) {
        qDebug() << "Purging gecko startupCache at" << cache.absoluteFilePath();
        QFile::remove(cache.absoluteFilePath());
    }
    QMozContext::instance()->addComponentManifest(componentPath + QString("components/EmbedLiteBinComponents.manifest"));
    QMozContext::instance()->addComponentManifest(componentPath + QString("components/EmbedLiteJSComponents.manifest"));
    QMozContext::instance()->addComponentManifest(componentPath + QString("chrome/EmbedLiteJSScripts.manifest"));
    QMozContext::instance()->addComponentManifest(componentPath + QString("chrome/EmbedLiteOverrides.manifest"));
    QMozContext::instance()->addComponentManifest(manifest.absoluteFilePath());
    // EngineSettings takes care of proper timing
    QMozEngineSettings::instance()->setPreference("security.tls.version.min",1);
    // run gecko thread
    QTimer::singleShot(0, QMozContext::instance(), SLOT(runEmbedding()));

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty("version", QStringLiteral(VERSION));
    view->rootContext()->setContextProperty("locale", locale);
    view->rootContext()->setContextProperty("MozContext", QMozContext::instance());
    view->rootContext()->setContextProperty("appFilePath",QCoreApplication::applicationFilePath());


    view->setSource(SailfishApp::pathTo("qml/rockpool.qml"));
    view->show();

    return app->exec();
}
