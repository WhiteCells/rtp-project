#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpipv4address.h>

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>

using namespace jrtplib;
using namespace std;

// ----------------------- Frame & Types -----------------------
enum pjmedia_frame_type {
    PJMEDIA_FRAME_TYPE_NONE,
    PJMEDIA_FRAME_TYPE_AUDIO,
    PJMEDIA_FRAME_TYPE_VIDEO
};

typedef std::vector<uint8_t> ByteVector;

struct MediaFrame
{
    pjmedia_frame_type type;
    ByteVector buf;
    unsigned size;

    MediaFrame() :
        type(PJMEDIA_FRAME_TYPE_NONE), size(0) {}
};

// ----------------------- Error Checking -----------------------
#define CHECK_ERROR(status)                                     \
    if ((status) < 0) {                                         \
        cerr << "ERROR: " << RTPGetErrorString(status) << endl; \
        exit(-1);                                               \
    }

// ----------------------- Threads -----------------------
std::atomic<bool> running {true};

void senderThread(RTPSession *session, MediaFrame frame)
{
    while (running) {
        std::cout << ">>> s" << std::endl;
        int status = session->SendPacket(frame.buf.data(), frame.size, 96, true, 160);
        CHECK_ERROR(status);
        // cout << "[发送] RTP包，大小=" << frame.size << " bytes" << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void receiverThread(RTPSession *session)
{
    while (running) {
        session->Poll();
        session->BeginDataAccess();

        std::cout << ">>> r" << std::endl;

        while (session->GotoNextSourceWithData()) {
            RTPPacket *pack;
            std::cout << "<<<received>>>" << std::endl;
            while ((pack = session->GetNextPacket()) != nullptr) {
                cout << "[接收] RTP包，大小=" << pack->GetPayloadLength()
                     << " bytes, 序列号=" << pack->GetSequenceNumber() << endl;
                session->DeletePacket(pack);
            }
        }

        session->EndDataAccess();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

// ----------------------- RTP Setup & Main -----------------------
int main(int argc, char *argv[])
{
    if (argc != 4) {
        std::cerr << "用法: " << argv[0] << " <目标IP> <目标端口> <本地端口>" << std::endl;
        std::cerr << "示例: " << argv[0] << " 127.0.0.1 6000 5000" << std::endl;
        return -1;
    }

    std::string destIP = argv[1];
    uint16_t destPort = static_cast<uint16_t>(std::stoi(argv[2]));
    uint16_t localPort = static_cast<uint16_t>(std::stoi(argv[3]));

    // RTP 会话
    RTPSession session;

    RTPSessionParams sessionparams;
    sessionparams.SetOwnTimestampUnit(1.0 / 8000.0); // 8kHz
    sessionparams.SetAcceptOwnPackets(true);         // 本地测试接收自己发的包

    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(localPort);

    int status = session.Create(sessionparams, &transparams);
    CHECK_ERROR(status);

    uint32_t destip = inet_addr(destIP.c_str());
    destip = ntohl(destip); // JRTPLIB 要求主机字节序
    status = session.AddDestination(RTPIPv4Address(destip, destPort));
    CHECK_ERROR(status);

    // 伪造音频帧
    MediaFrame frame;
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.size = 160;                   // 20ms @ G.711 (8kHz)
    frame.buf.resize(frame.size, 0xAB); // 填充假数据

    cout << "RTP会话已启动，目标: " << destIP << ":" << destPort
         << "，本地监听端口: " << localPort << endl;

    // 启动线程
    std::thread recvThread(receiverThread, &session);
    std::thread sendThread(senderThread, &session, frame);

    // 运行时长控制（可替换为外部停止条件）
    std::this_thread::sleep_for(std::chrono::minutes(5));
    running = false;

    recvThread.join();
    sendThread.join();

    session.BYEDestroy(RTPTime(10, 0), "Session ended", strlen("Session ended"));
    cout << "RTP会话结束。" << endl;

    return 0;
}
