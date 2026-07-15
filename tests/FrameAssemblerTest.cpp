#include <QtTest/QtTest>

#include "FrameAssembler.h"

class FrameAssemblerTest : public QObject {
    Q_OBJECT

private slots:
    void preservesV3ScanDirectionAfterV4Upgrade();
};

void FrameAssemblerTest::preservesV3ScanDirectionAfterV4Upgrade()
{
    qRegisterMetaType<StreamFrame>();
    FrameAssembler assembler;
    QSignalSpy spy(&assembler, &FrameAssembler::frameReady);

    cli::proto::StreamHeader header = {};
    header.magic = cli::proto::kStreamMagic;
    header.version = cli::proto::kStreamMetadataProtoVersion;
    header.channel = cli::proto::Raw16;
    header.flags = cli::proto::LastFragment;
    header.frame_id = 7;
    header.pkt_index = 0;
    header.pkt_total = 1;
    header.frame_bytes = 2;
    header.width = 1;
    header.height = 1;
    header.pixel_format = cli::proto::Mono16;
    header.meta_flags = cli::proto::HasRawFrameMeta | cli::proto::HasScanDirection;
    header.frame_type = cli::proto::DataFrame;
    header.reverse_scan = 1;

    const char payload[2] = {0x34, 0x12};
    assembler.feedPacket(header, payload, sizeof(payload));

    QCOMPARE(spy.count(), 1);
    const StreamFrame frame = qvariant_cast<StreamFrame>(spy.takeFirst().at(0));
    QVERIFY(frame.hasScanDirection);
    QVERIFY(frame.reverseScan);
    QCOMPARE(frame.streamFrameId, static_cast<quint64>(7));
    QCOMPARE(frame.data, QByteArray(payload, sizeof(payload)));
}

QTEST_MAIN(FrameAssemblerTest)

#include "FrameAssemblerTest.moc"
