#include "config.h"
#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // setup & load config
    config = new Config();
    config->readSettings();
    loadSettings();

    // setup & start server
    server = new QProcess();
    connect(server, SIGNAL(readyReadStandardOutput()), this, SLOT(on_readoutput()));
    connect(server, SIGNAL(readyReadStandardError()), this, SLOT(on_readerror()));
    startServer();

    // setup system tray
    QSystemTrayIcon *tray = new QSystemTrayIcon(this);
    tray->setIcon(QIcon(":/res/icon.ico"));
    tray->setToolTip("QtUnblockNeteaseMusic");
    QMenu *trayMenu = new QMenu(this);
    QAction *trayExit = new QAction(this);
    QAction *trayShow = new QAction(this);
    trayShow->setText(tr("Show"));
    trayExit->setText(tr("Exit"));
    trayMenu->addAction(trayShow);
    trayMenu->addAction(trayExit);
    tray->setContextMenu(trayMenu);
    tray->show();
    connect(trayShow, &QAction::triggered, this, [this]
            {
                this->show();
                this->activateWindow(); });
    connect(trayExit, &QAction::triggered, this, [this]
            {
                stopServer();
                QApplication::exit(); });
    // show MainWindow only when tray icon is left clicked
    connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::Trigger)
                {
                    this->show();
                    this->activateWindow();
                } });

    // setup theme menu
    for (QString &style : QStyleFactory::keys())
    {
        qDebug() << "Loading theme" << style;
        // reference: https://stackoverflow.com/a/45265455
        connect(ui->menuTheme->addAction(style), &QAction::triggered, this, [style]
                {
                    qDebug() << "Setting theme" << style;
                    QApplication::setStyle(QStyleFactory::create(style));
                    QApplication::setPalette(QApplication::style()->standardPalette()); });
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionExit_triggered()
{
    stopServer();
    QApplication::exit();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox *aboutDlg = new QMessageBox(this);
    const QString text = tr("<h3>About QtUnblockNeteaseMusic</h3>"
                            "<p>Version %1</p>")
                             .arg(QApplication::applicationVersion());
    const QString informativeText = tr("<p>A desktop client for UnblockNeteaseMusic, "
                                       "written in Qt.</p>"
                                       "<p>Copyright 2022 FrzMtrsprt</p>");
    aboutDlg->setAttribute(Qt::WA_DeleteOnClose);
    aboutDlg->setWindowTitle("About");
    aboutDlg->setIconPixmap(QPixmap(":/res/icon.png").scaledToHeight(100, Qt::SmoothTransformation));
    aboutDlg->setText(text);
    aboutDlg->setInformativeText(informativeText);
    aboutDlg->setStandardButtons(QMessageBox::Ok | QMessageBox::Help);
    aboutDlg->setEscapeButton(QMessageBox::Ok);
    aboutDlg->button(QMessageBox::Help)->setText("GitHub");
    if (aboutDlg->exec() == QMessageBox::Help)
    {
        QDesktopServices::openUrl(QUrl(QApplication::organizationDomain()));
    }
}

void MainWindow::on_actionAboutQt_triggered()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::on_startupCheckBox_stateChanged(const int state)
{
    config->setStartup(state);
    updateSettings();
    config->writeSettings();
}

void MainWindow::on_readoutput()
{
    const QByteArray &logArray = server->readAllStandardOutput();
    outLog(logArray.data());
}

void MainWindow::on_readerror()
{
    QMessageBox errorDlg(this);
    const QByteArray &logArray = server->readAllStandardError();
    errorDlg.setWindowTitle(tr("Server error"));
    errorDlg.setText(tr("The UnblockNeteaseMusic server ran into an error.\n"
                        "Please change the arguments or check port usage and try again."));
    errorDlg.setDetailedText(logArray.data());
    errorDlg.setIcon(QMessageBox::Warning);
    errorDlg.exec();
}

void MainWindow::on_restartBtn_clicked()
{
    stopServer();
    ui->outText->clear();
    startServer();
}

void MainWindow::on_exitBtn_clicked()
{
    stopServer();
    QApplication::exit();
}

void MainWindow::loadSettings()
{
    // load settings from variables into ui
    ui->portEdit->setText(config->port);
    ui->addressEdit->setText(config->address);
    ui->urlEdit->setText(config->url);
    ui->hostEdit->setText(config->host);
    ui->sourceEdit->append(config->source);
    ui->strictCheckBox->setChecked(config->strict);
    ui->startupCheckBox->setChecked(config->startup);
}

void MainWindow::updateSettings()
{
    // update settings from ui into variables
    config->port = ui->portEdit->text();
    config->address = ui->addressEdit->text();
    config->url = ui->urlEdit->text();
    config->host = ui->hostEdit->text();
    config->source = ui->sourceEdit->toPlainText();
    config->strict = ui->strictCheckBox->isChecked();
    config->startup = ui->startupCheckBox->isChecked();
}

bool MainWindow::getServer(QString &serverFile, QStringList &serverArgs)
{
    QDir appDir(QApplication::applicationDirPath());
    appDir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks);
    appDir.setNameFilters({"unblock*", "server*"});
    if (appDir.count())
    {
        QFileInfo result(QApplication::applicationDirPath(), appDir[0]);
        if (result.isFile())
        {
            serverFile = result.absoluteFilePath();
            serverArgs = {};
        }
        if (result.isDir())
        {
            QDir serverDir(result.absoluteFilePath());
            if (serverDir.exists("app.js"))
            {
                serverFile = "node";
                serverArgs = {serverDir.absolutePath() + "/app.js"};
            }
            else
            {
                return false;
            }
        }
        qDebug() << "Server File:" << serverFile.toUtf8().data();
        return true;
    }
    else
    {
        serverFile = "";
        serverArgs = {};
        return false;
    }
}

void MainWindow::getArgs(QStringList &serverArgs)
{
    if (!ui->portEdit->text().isEmpty())
    {
        serverArgs << "-p" << ui->portEdit->text();
    }
    if (!ui->addressEdit->text().isEmpty())
    {
        serverArgs << "-a" << ui->addressEdit->text();
    }
    if (!ui->urlEdit->text().isEmpty())
    {
        serverArgs << "-u" << ui->urlEdit->text();
    }
    if (!ui->hostEdit->text().isEmpty())
    {
        serverArgs << "-f" << ui->hostEdit->text();
    }
    if (!ui->sourceEdit->toPlainText().isEmpty())
    {
        const QString source = ui->sourceEdit->toPlainText();
        static QRegularExpression sep("[,.'，。 \n]+");
        QStringList sources = source.split(sep);
        sources.removeAll("");
        serverArgs << "-o" << sources;
    }
    if (ui->strictCheckBox->isChecked())
    {
        serverArgs << "-s";
    }
    qDebug() << "Server Arguments:" << serverArgs.join(" ").toUtf8().data();
}

void MainWindow::startServer()
{
    QString serverFile = "";
    QStringList serverArgs = {};
    if (getServer(serverFile, serverArgs))
    {
        getArgs(serverArgs);
        server->start(serverFile, serverArgs);
        if (!server->waitForStarted())
        {
            outLog(server->errorString());
        }
    }
    else
    {
        outLog(tr("Server not found."));
    }
}

// reference: https://github.com/barry-ran/QtScrcpy/blob/3929ebf62ee0eb594d566e570a79ccb8efe6bb60/QtScrcpy/ui/dialog.cpp#L359
void MainWindow::outLog(const QString &log)
{
    // avoid sub thread update ui
    QString backLog = log;
    QTimer::singleShot(0, this, [this, backLog]()
                       { ui->outText->append(backLog); });
}

void MainWindow::stopServer()
{
    server->close();
    updateSettings();
    config->writeSettings();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    this->hide();
    e->ignore();
}
