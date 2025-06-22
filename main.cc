#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpipv4address.h>

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>

using namespace jrtplib;
using namespace std;

enum pjmedia_frame_type {
    PJMEDIA_FRAME_TYPE_NONE,
    PJMEDIA_FRAME_TYPE_AUDIO,
    PJMEDIA_FRAME_TYPE_VIDEO
};

typedef std::vector<uint8_t> ByteVector;

struct MediaFrame
{
    pjmedia_frame_type type;
    ByteVector         buf;
    unsigned           size;

    MediaFrame()
        : type(PJMEDIA_FRAME_TYPE_NONE), size(0) {}
};

#define CHECK_ERROR(status) \
    if ((status) < 0) { \
        cerr << "ERROR: " << RTPGetErrorString(status) << endl; \
        exit(-1); \
    }

void receiveAndSendRTP(const std::string& destIP, uint16_t destPort, uint16_t localPort) {
    RTPSession session;

    // 1. 配置会话参数（时间戳单位1/8000，音频采样率8kHz）
    RTPSessionParams sessionparams;
    sessionparams.SetOwnTimestampUnit(1.0 / 8000.0);
    sessionparams.SetAcceptOwnPackets(true);  // 允许接收自己发送的包（调试用）

    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(localPort);

    int status = session.Create(sessionparams, &transparams);
    CHECK_ERROR(status);

    uint32_t destip = inet_addr(destIP.c_str());
    status = session.AddDestination(RTPIPv4Address(destip, destPort));
    CHECK_ERROR(status);

    // 2. 构造一个示例MediaFrame数据
    MediaFrame frame;
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.size = 160;
    frame.buf.resize(frame.size, 0xAB);

    cout << "开始发送和接收RTP数据..." << endl;

    // 3. 运行主循环：接收并打印RTP包，同时每隔1秒发送一个包
    int64_t lastSendTime = 0;

    while (true) {
        session.Poll();
        session.BeginDataAccess();

        while (session.GotoNextSourceWithData()) {
            RTPPacket *pack;
            while ((pack = session.GetNextPacket()) != nullptr) {
                cout << "收到RTP包，大小=" << pack->GetPayloadLength() << " bytes, 序列号=" << pack->GetSequenceNumber() << endl;

                // 你可以在这里解析pack->GetPayloadData()，或者调用你的解码器

                session.DeletePacket(pack);
            }
        }

        session.EndDataAccess();

        // 定时发送包 (每1000ms)
        auto now = chrono::steady_clock::now().time_since_epoch();
        int64_t nowMs = chrono::duration_cast<chrono::milliseconds>(now).count();
        if (nowMs - lastSendTime > 1000) {
            status = session.SendPacket(frame.buf.data(), frame.size, 96, true, 160);
            CHECK_ERROR(status);
            cout << "发送RTP包，大小=" << frame.size << " bytes" << endl;
            lastSendTime = nowMs;
        }

        this_thread::sleep_for(chrono::milliseconds(20));
    }

    // 关闭会话（一般不会到这里）
    session.BYEDestroy(RTPTime(10, 0), "Session ended", strlen("Session ended"));
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "用法: " << argv[0] << " <目标IP> <目标端口> <本地端口>" << std::endl;
        std::cerr << "示例: " << argv[0] << " 127.0.0.1 60000 50000" << std::endl;
        return -1;
    }

    std::string destIP = argv[1];
    uint16_t destPort = static_cast<uint16_t>(std::stoi(argv[2]));
    uint16_t localPort = static_cast<uint16_t>(std::stoi(argv[3]));

    receiveAndSendRTP(destIP, destPort, localPort);

    return 0;
}
