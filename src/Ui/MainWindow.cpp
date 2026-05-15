#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QSettings>
#include <QCloseEvent>
#include "DeviceClient.h"
#include "panels/ConnectionPanel.h"
#include "panels/CameraPanel.h"
#include "panels/MirrorPanel.h"
#include "panels/IrPanel.h"
#include "panels/CollectPanel.h"
#include "panels/StreamPanel.h"
#include "panels/LogPanel.h"
#include "widgets/ImageView.h"
#include "widgets/StatusBarPanel.h"
#include "Protocol.h"
#include "plog/Log.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("AtingSpectrograph Client");
    resize(1400, 850);
    setMinimumSize(1000, 600);

    device_ = new DeviceClient(this);
    setupUi();

    // connection state -> status bar
    connect(device_, &DeviceClient::connectionChanged, this,
            [this](bool c, const QString& ip) {
        statusPanel_->setConnected(c);
        statusBar()->showMessage(c ? (QString::fromUtf8("已连接 ") + ip)
                                    : QString::fromUtf8("未连接"));
    });

    // mirror event -> status bar
    connect(device_, &DeviceClient::mirrorAngleEvent, this,
            [this](double angle, bool, qint64) {
        statusPanel_->setMirrorAngle(angle);
    });

    // frame -> ImageView + stats
    connect(device_, &DeviceClient::frameReady, this,
            [this](int channel, int width, int height, int pixfmt, QByteArray data) {
        int tabIdx = -1;
        using namespace cli::proto;
        switch (channel) {
        case Raw16:          tabIdx = 0; break;
        case SliceStitch16:  tabIdx = 1; break;
        default: return;
        }

        LOGD << "frameReady: channel=" << channel << ", width=" << width << ", height=" << height << ", pixfmt=" << pixfmt << ", data.size=" << data.size();

        QImage img;
        if (pixfmt == Mono8) {
            img = QImage(reinterpret_cast<const uchar*>(data.constData()),
                         width, height, width, QImage::Format_Grayscale8).copy();
        } else {
            int pixels = width * height;
            if (data.size() < pixels * 2) return;
            const uint16_t* src = reinterpret_cast<const uint16_t*>(data.constData());
            uint16_t mn = 0xFFFF, mx = 0;
            for (int i = 0; i < pixels; ++i) {
                if (src[i] < mn) mn = src[i];
                if (src[i] > mx) mx = src[i];
            }
            QByteArray buf8(pixels, '\0');
            if (mx > mn) {
                double s = 255.0 / (mx - mn);
                for (int i = 0; i < pixels; ++i)
                    buf8[i] = static_cast<char>(static_cast<uint8_t>((src[i] - mn) * s));
            }
            img = QImage(reinterpret_cast<const uchar*>(buf8.constData()),
                         width, height, width, QImage::Format_Grayscale8).copy();
        }

        if (imageViews_[tabIdx])
            imageViews_[tabIdx]->setImage(img);

        statusPanel_->setFps(device_->stream()->fps());
        statusPanel_->setFrames(device_->stream()->framesReceived());
        statusPanel_->setDropped(device_->stream()->framesDropped());
    });

    // TCP raw log -> log panel
    connect(device_->control(), &ControlClient::rawLog, this,
            [this](const QString& line) {
        if (logPanel_) logPanel_->appendLog(line);
    });

    loadSettings();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* e)
{
    saveSettings();
    device_->disconnect();
    QMainWindow::closeEvent(e);
}

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setupSidebar();
    layout->addWidget(sidebar_);

    mainSplitter_ = new QSplitter(Qt::Horizontal, this);

    setupCentral();
    mainSplitter_->addWidget(imageTabs_);

    setupPanels();
    mainSplitter_->addWidget(panelStack_);

    mainSplitter_->setStretchFactor(0, 1);
    mainSplitter_->setStretchFactor(1, 0);
    layout->addWidget(mainSplitter_, 1);

    setupStatusBar();
}

void MainWindow::setupSidebar()
{
    sidebar_ = new QListWidget(this);
    sidebar_->setFixedWidth(120);
    sidebar_->setIconSize(QSize(20, 20));

    QStringList items = {
        QString::fromUtf8("连接"),
        QString::fromUtf8("相机"),
        QString::fromUtf8("转镜"),
        QString::fromUtf8("红外"),
        QString::fromUtf8("采集"),
        QString::fromUtf8("通道"),
        QString::fromUtf8("日志"),
    };
    sidebar_->addItems(items);
    sidebar_->setCurrentRow(0);

    connect(sidebar_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (panelStack_ && row >= 0 && row < panelStack_->count())
            panelStack_->setCurrentIndex(row);
    });
}

void MainWindow::setupCentral()
{
    imageTabs_ = new QTabWidget(this);
    QStringList chNames = {"Raw16", "SliceStitch16"};
    for (int i = 0; i < 2; ++i) {
        imageViews_[i] = new ImageView(this);
        imageTabs_->addTab(imageViews_[i], chNames[i]);
    }
}

void MainWindow::setupPanels()
{
    panelStack_ = new QStackedWidget(this);
    panelStack_->setMinimumWidth(320);
    panelStack_->setMaximumWidth(460);

    connPanel_    = new ConnectionPanel(device_, this);
    cameraPanel_  = new CameraPanel(device_, this);
    mirrorPanel_  = new MirrorPanel(device_, this);
    irPanel_      = new IrPanel(device_, this);
    collectPanel_ = new CollectPanel(device_, this);
    streamPanel_  = new StreamPanel(device_, this);
    logPanel_     = new LogPanel(this);

    panelStack_->addWidget(connPanel_);    // 0
    panelStack_->addWidget(cameraPanel_);  // 1
    panelStack_->addWidget(mirrorPanel_);  // 2
    panelStack_->addWidget(irPanel_);      // 3
    panelStack_->addWidget(collectPanel_); // 4
    panelStack_->addWidget(streamPanel_);  // 5
    panelStack_->addWidget(logPanel_);     // 6

    // stream integration
    connect(streamPanel_, &StreamPanel::subscribeRequested, this,
            [this](quint16 port, const QStringList&) {
        if (!device_->stream()->isBound())
            device_->stream()->bind(port);
    });
    connect(device_, &DeviceClient::connectionChanged, this,
            [this](bool connected, const QString&) {
        if (connected) {
            quint16 port = connPanel_->udpPort();
            streamPanel_->setUdpPort(port);
            if (!device_->stream()->isBound())
                device_->stream()->bind(port);
        }
    });
}

void MainWindow::setupStatusBar()
{
    statusPanel_ = new StatusBarPanel(this);
    statusBar()->addPermanentWidget(statusPanel_, 1);
    statusBar()->showMessage(QString::fromUtf8("就绪"));
}

void MainWindow::loadSettings()
{
    QSettings s;
    restoreGeometry(s.value("window/geometry").toByteArray());
    if (mainSplitter_)
        mainSplitter_->restoreState(s.value("window/splitter").toByteArray());
    int panel = s.value("window/panel", 0).toInt();
    if (panel >= 0 && panel < panelStack_->count()) {
        sidebar_->setCurrentRow(panel);
        panelStack_->setCurrentIndex(panel);
    }
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue("window/geometry", saveGeometry());
    if (mainSplitter_)
        s.setValue("window/splitter", mainSplitter_->saveState());
    s.setValue("window/panel", sidebar_->currentRow());
}
