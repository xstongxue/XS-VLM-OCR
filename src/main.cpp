#include "ui/MainWindow.h"
#include "core/OCRResult.h"
#include <QApplication>
#include <QTextCodec>
#include <QIcon>
#include <QDebug>

#ifdef _WIN32
#pragma comment(lib, "user32.lib")
#include <windows.h>
#endif

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