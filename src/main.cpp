#include "ui/MainWindow.h"
#include "core/OCRResult.h"
#include <QApplication>
#include <QTextCodec>
#include <QIcon>
#include <QDebug>
#include <QMessageLogContext>

#ifdef _WIN32
#pragma comment(lib, "user32.lib")
#include <windows.h>
#endif

// 发布版不向终端输出 qDebug，仅保留 qWarning/qCritical/qFatal；可通过环境变量 XS_VLM_OCR_DEBUG=1 开启调试日志
static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (type == QtDebugMsg && qEnvironmentVariableIsSet("XS_VLM_OCR_DEBUG") == false) {
        return;
    }
    QString formatted = qFormatLogMessage(type, context, msg);
    fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Windows 下设置控制台为 UTF-8 编码, 防止中文乱码
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IONBF, 0);
#endif
    // 设置 UTF-8 编码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));  
    
    // 启用高DPI缩放
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    qInstallMessageHandler(messageHandler);

    // 注册自定义类型，用于跨线程信号槽
    qRegisterMetaType<OCRResult>("OCRResult");
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<SubmitSource>("SubmitSource");

    // 设置应用信息
    app.setApplicationName("XS-VLM-OCR");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("XS-VLM-OCR Team");
    
    // 设置应用程序图标
    QIcon appIcon(":/res/favicon.ico");
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}