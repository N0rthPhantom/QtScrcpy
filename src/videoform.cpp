#include <QDesktopWidget>
#include <QMouseEvent>

#include "videoform.h"
#include "ui_videoform.h"
#include "inputconvert.h"

VideoForm::VideoForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::videoForm)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    m_server = new Server();
    m_frames.init();
    m_decoder.setFrames(&m_frames);

    connect(m_server, &Server::serverStartResult, this, [this](bool success){
        if (success) {
            m_server->connectTo();
        }
    });

    connect(m_server, &Server::connectToResult, this, [this](bool success, const QString &deviceName, const QSize &size){
        if (success) {
            // update ui
            setWindowTitle(deviceName);
            updateShowSize(size);
            QDesktopWidget* desktop = QApplication::desktop();
            if (desktop) {
                QRect mainScreenRc = desktop->availableGeometry();
                move((mainScreenRc.width() - width())/2, (mainScreenRc.height() - height())/2);
            }

            // init decode
            m_decoder.setDeviceSocket(m_server->getDeviceSocket());
            m_decoder.startDecode();

            // init controller
            m_controller.setDeviceSocket(m_server->getDeviceSocket());
        }
    });

    connect(m_server, &Server::onServerStop, this, [this](){
        close();
        qDebug() << "server process stop";
    });

    connect(&m_decoder, &Decoder::onDecodeStop, this, [this](){
        close();
        qDebug() << "decoder thread stop";
    });

    // must be Qt::QueuedConnection, ui update must be main thread
    QObject::connect(&m_decoder, &Decoder::onNewFrame, this, [this](){
        m_frames.lock();
        const AVFrame *frame = m_frames.consumeRenderedFrame();
        updateShowSize(QSize(frame->width, frame->height));
        ui->videoWidget->setFrameSize(QSize(frame->width, frame->height));
        ui->videoWidget->updateTextures(frame->data[0], frame->data[1], frame->data[2], frame->linesize[0], frame->linesize[1], frame->linesize[2]);
        m_frames.unLock();
    },Qt::QueuedConnection);

    m_server->start("P7C0218510000537", 27183, 1080, 8000000, "");
    //m_server->start("P7C0218510000537", 27183, 0, 8000000, "");
}

VideoForm::~VideoForm()
{
    m_server->stop();
    m_decoder.stopDecode();
    delete m_server;
    m_frames.deInit();
    delete ui;
}

void VideoForm::updateShowSize(const QSize &newSize)
{
    QSize showSize = newSize;
    QDesktopWidget* desktop = QApplication::desktop();
    if (desktop) {
        QSize screenSize = desktop->size();
        showSize.setWidth(qMin(newSize.width(), screenSize.width()));
        showSize.setHeight(qMin(newSize.height(), screenSize.height() - 100));
    }

    if (showSize != size()) {
        resize(showSize);
    }
}

void VideoForm::mousePressEvent(QMouseEvent *event)
{
    ControlEvent* controlEvent = InputConvert::mouseEvent(event, ui->videoWidget->frameSize(), size());
    if (controlEvent) {
        m_controller.postControlEvent(controlEvent);
    }
}

void VideoForm::mouseReleaseEvent(QMouseEvent *event)
{
    ControlEvent* controlEvent = InputConvert::mouseEvent(event, ui->videoWidget->frameSize(), size());
    if (controlEvent) {
        m_controller.postControlEvent(controlEvent);
    }
}

void VideoForm::mouseMoveEvent(QMouseEvent *event)
{
    ControlEvent* controlEvent = InputConvert::mouseEvent(event, ui->videoWidget->frameSize(), size());
    if (controlEvent) {
        m_controller.postControlEvent(controlEvent);
    }
}

void VideoForm::wheelEvent(QWheelEvent *event)
{
    ControlEvent* controlEvent = InputConvert::wheelEvent(event, ui->videoWidget->frameSize(), size());
    if (controlEvent) {
        m_controller.postControlEvent(controlEvent);
    }
}