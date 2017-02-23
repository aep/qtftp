#include <QUdpSocket>
#include <QCoreApplication>
#include <QDebug>
#include <QtEndian>
#include <QFile>
#include <QFileInfo>
#include <iostream>



QByteArray hcFile;
class TftpServer : public QObject
{
    Q_OBJECT
public:
    TftpServer() : QObject()
    {
        udpSocket = new QUdpSocket(this);
        if (!udpSocket->bind(QHostAddress::Any, 69, QUdpSocket::DontShareAddress)) {
            qFatal("cant bind");
        }

        connect(udpSocket, SIGNAL(readyRead()),
                this, SLOT(readPendingDatagrams()));
        state = 0;
        blockNr = 0;
    }
private:
    int state;
    int blockNr;
    int totalBlocks;
    QHostAddress sender;
    quint16 senderPort;
    QFile ff;

    QUdpSocket * udpSocket;
    void processTheDatagram(QByteArray dg) {
        uint16_t opcode = qFromBigEndian(*(uint16_t*)dg.data());
        dg.remove(0,2);
        switch (opcode) {
            case 2:
                error(4, "write request not supported");
                break;
            case 3:
                error(4, "why are you sending me data?");
                break;
            case 4: {
                        uint16_t ackd = qFromBigEndian(*(uint16_t*)dg.data());
                        if (ackd != blockNr) {
                            qDebug() << "got ack for wrong block";
                            return;
                        }
                        sendData();
                    }
                    break;
            case 1:{
                       if (state != 0) {
                           error(4, "wrong state to request files");
                           return;
                       }
                       int sl = strlen(dg.data());
                       qDebug() << sl << dg;
                       QByteArray fileName = dg.left(sl);
                       dg.remove(0, sl + 1);
                       sl = strlen(dg.data());
                       QByteArray mode = dg.left(sl);
                       qDebug() <<"file request:" << fileName << mode;
                       if (mode != "octet") {
                           error(4, "can only do octet mode");
                           return;
                       }
                       fileRequest(fileName);
                   }
                   break;
            default:
                qDebug() << opcode << dg;
                break;
        }
    }
    void fileRequest(QByteArray fileName)
    {
        if (fileName.contains("/")) {
            error(1, "file contains slash. not doing");
            return;
        }
        if (!hcFile.isEmpty()) {
            fileName = hcFile;
        }
        ff.setFileName(fileName);
        if (!ff.open(QFile::ReadOnly)) {
            error(1, "cant open" + fileName);
            return;
        }
        totalBlocks = QFileInfo(fileName).size() / 512;
        state = 1;
        sendData();
    }
    void sendData() {
        char b[512 + 4];
        *((uint16_t*)b + 0) = qToBigEndian<uint16_t>(3);
        *((uint16_t*)b + 1) = qToBigEndian<uint16_t>(++blockNr);
        int rs = ff.read(b + 4, 512);


        if (udpSocket->writeDatagram(b, rs + 4, sender, senderPort) < rs +4 ) {
            qDebug() << "send dgm went wrong";
        }

        std::cerr << (blockNr -1) << "/" << totalBlocks << "\r";
        if (rs < 1) {
            std::cerr << "\n";
            exit(0);
        }

    }

    void error(uint16_t code, const QByteArray &msg) {
        qDebug() << msg;
        char b[512];
        memset(b,0,512);
        *((uint16_t*)b + 0) = qToBigEndian<uint16_t>(5);
        *((uint16_t*)b + 1) = qToBigEndian<uint16_t>(code);
        strncpy(b + 4, msg.data(), 512 - 4);
        if (udpSocket->writeDatagram(b, 512, sender, senderPort) < 512 ) {
            qDebug() << "send dgm went wrong";
        }
    }
private slots:

    void readPendingDatagrams()
    {
        while (udpSocket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(udpSocket->pendingDatagramSize());

            udpSocket->readDatagram(datagram.data(), datagram.size(),
                    &sender, &senderPort);

            processTheDatagram(datagram);
        }
    }

};


int main(int argc, char **argv)
{
    QCoreApplication app(argc,argv);
    if (app.arguments().size() > 1) {
        hcFile = app.arguments().at(1).toLocal8Bit();
        qDebug() << "always responding with file " << hcFile;
    }

    TftpServer s;


    return app.exec();
}


#include "main.moc"
