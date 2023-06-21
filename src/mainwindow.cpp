#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "envdialog.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFontDatabase>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

#ifdef Q_OS_WIN
#include "utils/winutils.h"
#endif

using namespace Qt::Literals::StringLiterals;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
#ifdef Q_OS_WIN
    QFont font = QFont(u"Consolas"_s);
    font.setStyleHint(QFont::TypeWriter);
#else
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#endif
    ui->outText->setFont(font);

    // connect MainWindow signals
    connect(ui->actionInstallCA, &QAction::triggered,
            this, &MainWindow::on_installCA);
    connect(ui->actionEnv, &QAction::triggered,
            this, &MainWindow::on_env);
    connect(ui->actionExit, &QAction::triggered,
            this, &MainWindow::exit);
    connect(ui->actionAbout, &QAction::triggered,
            this, &MainWindow::on_about);
    connect(ui->actionAboutQt, &QAction::triggered,
            this, &MainWindow::on_aboutQt);
    connect(ui->startupCheckBox, &QCheckBox::clicked,
            this, &MainWindow::on_startup);
    connect(ui->proxyCheckBox, &QCheckBox::clicked,
            this, &MainWindow::setProxy);
    connect(ui->applyBtn, &QPushButton::clicked,
            this, &MainWindow::on_apply);
    connect(ui->exitBtn, &QPushButton::clicked,
            this, &MainWindow::exit);

    // setup theme menu
    for (const QString &style : QStyleFactory::keys())
    {
        // reference: https://stackoverflow.com/a/45265455
        QAction *action = ui->menuTheme->addAction(style);
        connect(action, &QAction::triggered,
                this, [this, style]
                { setTheme(style); });
    }

    // setup & load config
    config = new Config();
    loadSettings();

    // setup & start server
    qDebug("---Starting server---");
    server = new Server(ui->outText, config);
    QTimer::singleShot(0, server, &Server::start);
}

MainWindow::~MainWindow()
{
    delete ui;
    server->~Server();
    config->~Config();
}

void MainWindow::setTheme(const QString &theme)
{
    QStyle *style = QStyleFactory::create(theme);
    if (style)
    {
#ifdef Q_OS_WIN
        WinUtils::setWindowFrame(winId(), style);
#endif
        QApplication::setStyle(style);
        style->name() == u"windowsvista"_s || style->name() == u"macOS"_s
            // Do not set palette for native styles
            ? QApplication::setPalette(QPalette())
            : QApplication::setPalette(style->standardPalette());
    }
}

bool MainWindow::setProxy(const bool &enable)
{
    const QString address = config->params[Param::Address].value<QString>();
    const QString port = config->params[Param::Port].value<QString>().split(u':')[0];
    bool ok = false;
#ifdef Q_OS_WIN
    ok = WinUtils::setSystemProxy(enable, address, port);
#endif
    if (!ok)
    {
        ui->proxyCheckBox->setChecked(isProxy());

        const QString title = tr("Error");
        const QString text =
            tr("Failed to set system proxy.\n"
               "Please check the server port "
               "and address, and try again.");

        QMessageBox *errorDlg = new QMessageBox(this);
        errorDlg->setAttribute(Qt::WA_DeleteOnClose);
        errorDlg->setWindowTitle(title);
        errorDlg->setText(text);
        errorDlg->setIcon(QMessageBox::Warning);
        errorDlg->exec();
    }
    return ok;
}

bool MainWindow::isProxy()
{
    const QString address = config->params[Param::Address].value<QString>();
    const QString port = config->params[Param::Port].value<QString>().split(u':')[0];
    bool isProxy = false;
#ifdef Q_OS_WIN
    isProxy = WinUtils::isSystemProxy(address, port);
#endif
    return isProxy;
}

void MainWindow::exit()
{
    qDebug("---Shutting down---");
    server->close();
    updateSettings();
    QApplication::exit();
}

void MainWindow::on_installCA()
{
    const QString caPath = u"./ca.crt"_s;
    QFile::copy(u":/certs/ca.crt"_s, caPath);
#ifdef Q_OS_WIN
    const auto [succeed, error, detail] = WinUtils::installCA(caPath);
#else
    const auto [succeed, error, detail] = std::make_tuple(false, u"Unsupported"_s, QString());
#endif
    QFile::setPermissions(caPath, QFileDevice::WriteOwner);
    QFile::remove(caPath);

    QMessageBox *dlg = new QMessageBox(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setIcon(succeed ? QMessageBox::Information
                         : QMessageBox::Warning);
    dlg->setText(error);
    dlg->setDetailedText(detail);
    dlg->exec();
}

void MainWindow::on_env()
{
    EnvDialog *envDlg = new EnvDialog(config, this);
    envDlg->setAttribute(Qt::WA_DeleteOnClose);
    envDlg->setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
    if (envDlg->exec() == QDialog::Accepted)
    {
        updateSettings();
        server->restart();
    }
}

void MainWindow::on_about()
{
    const QPixmap logo =
        QPixmap(u":/res/icon.png"_s)
            .scaled(100, 100,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);

    const QString text =
        tr("<h3>About %1</h3>"
           "<p>Version %2</p>")
            .arg(QApplication::applicationName(),
                 QApplication::applicationVersion());

    const QString info =
        tr("<p>A desktop client for UnblockNeteaseMusic, "
           "made with Qt.</p>"
           "<p>Copyright 2023 %1</p>")
            .arg(QApplication::organizationName());

    QMessageBox *aboutDlg = new QMessageBox(this);
    aboutDlg->setAttribute(Qt::WA_DeleteOnClose);
    aboutDlg->setWindowTitle(tr("About"));
    aboutDlg->setIconPixmap(logo);
    aboutDlg->setText(text);
    aboutDlg->setInformativeText(info);
    aboutDlg->setStandardButtons(QMessageBox::Ok);
    aboutDlg->setEscapeButton(QMessageBox::Ok);
    aboutDlg->addButton(QMessageBox::Help)->setText(u"GitHub"_s);

    if (aboutDlg->exec() == QMessageBox::Help)
    {
        const QUrl url(QApplication::organizationDomain());
        QDesktopServices::openUrl(url);
    }
}

void MainWindow::on_aboutQt()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::on_startup(const bool &enable)
{
#ifdef Q_OS_WIN
    WinUtils::setStartup(enable);
#endif
}

void MainWindow::on_apply()
{
    qDebug("---Restarting server---");
    const bool wasProxy = isProxy();
    updateSettings();
    server->restart();
    if (wasProxy)
    {
        setProxy(true);
    }
}

void MainWindow::loadSettings()
{
    qDebug("Loading settings");

    // load settings from file into variables
    config->readSettings();

    // load settings from variables into ui
    const QStringList split = config->params[Param::Port].value<QString>().split(u':');
    ui->httpEdit->setText(split[0]);
    ui->httpsEdit->setText(split.length() > 1 ? split[1] : u""_s);
    ui->addressEdit->setText(config->params[Param::Address].value<QString>());
    ui->urlEdit->setText(config->params[Param::Url].value<QString>());
    ui->hostEdit->setText(config->params[Param::Host].value<QString>());
    ui->sourceEdit->append(config->params[Param::Sources].value<QStringList>().join(u", "_s));
    ui->strictCheckBox->setChecked(config->params[Param::Strict].value<bool>());
    ui->startupCheckBox->setChecked(config->startup);
    ui->debugCheckBox->setChecked(config->debugInfo);
    setTheme(config->theme);

    qDebug("Load settings done");
}

void MainWindow::updateSettings()
{
    qDebug("Updating settings");

    static const QRegularExpression sep(u"\\W+"_s);

    // update settings from ui into variables
    const QString port = ui->httpsEdit->text().size()
                             ? ui->httpEdit->text() + u':' + ui->httpsEdit->text()
                             : ui->httpEdit->text();
    config->params[Param::Port].setValue(port);
    config->params[Param::Address].setValue(ui->addressEdit->text());
    config->params[Param::Url].setValue(ui->urlEdit->text());
    config->params[Param::Host].setValue(ui->hostEdit->text());
    config->params[Param::Sources].setValue(ui->sourceEdit->toPlainText().split(sep, Qt::SkipEmptyParts));
    config->params[Param::Strict].setValue(ui->strictCheckBox->isChecked());
    config->startup = ui->startupCheckBox->isChecked();
    config->debugInfo = ui->debugCheckBox->isChecked();
    config->theme = QApplication::style()->name();

    // write settings from variables into file
    config->writeSettings();

    qDebug("Update settings done");
}

// Event reloads
bool MainWindow::event(QEvent *e)
{
    switch (e->type())
    {
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent *>(e)->key() == Qt::Key_Escape)
        {
            hide();
        }
        break;
    case QEvent::Show:
#ifdef Q_OS_WIN
        WinUtils::setWindowFrame(winId(), style());
        WinUtils::setThrottle(false);
#endif
        break;
    case QEvent::Close:
        for (QDialog *dialog : findChildren<QDialog *>())
        {
            dialog->close();
        }
#ifdef Q_OS_WIN
        WinUtils::setThrottle(true);
#endif
        break;
    case QEvent::WindowActivate:
        ui->proxyCheckBox->setChecked(isProxy());
        break;
    case QEvent::ChildAdded:
#ifdef Q_OS_WIN
    {
        QObject *object = static_cast<QChildEvent *>(e)->child();
        // Set window border for child dialogs
        if (object->isWidgetType())
        {
            QWidget *widget = static_cast<QWidget *>(object);
            if (widget->isWindow())
            {
                WinUtils::setWindowFrame(widget->winId(), widget->style());
            }
        }
    }
#endif
    default:
        break;
    };
    return QMainWindow::event(e);
}
