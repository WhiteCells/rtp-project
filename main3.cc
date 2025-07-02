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
    ByteVector buf;
    unsigned size;

    MediaFrame() :
        type(PJMEDIA_FRAME_TYPE_NONE), size(0) {}
};

#define CHECK_ERROR(status)                                     \
    if ((status) < 0) {                                         \
        cerr << "ERROR: " << RTPGetErrorString(status) << endl; \
        exit(-1);                                               \
    }

/*
JRTPLIB 的 RTPSession 对象不是完全线程安全的。
不应该在多个线程中同时调用会话的成员函数来修改其内部状态，
例如在一个线程中调用 SendPacket()，同时在另一个线程中调用 Poll()。
*/
void processRTP(RTPSession *session, const MediaFrame& frame)
{
    const auto sendInterval = std::chrono::milliseconds(20);
    auto lastSendTime = std::chrono::steady_clock::now();
    bool running = true;

    cout << "开始 RTP 主循环..." << endl;

    while (running) {
        // ------------------ 1. 轮询和接收 ------------------
        session->Poll();

        session->BeginDataAccess();
        if (session->GotoFirstSourceWithData()) {
            do {
                RTPPacket *pack;
                while ((pack = session->GetNextPacket()) != nullptr) {
                    cout << "[接收] RTP包, 来自 SSRC " << pack->GetSSRC()
                         << ", 大小=" << pack->GetPayloadLength()
                         << " bytes, 序列号=" << pack->GetSequenceNumber() << endl;
                    session->DeletePacket(pack);
                }
            } while (session->GotoNextSourceWithData());
        }
        session->EndDataAccess();

        // ------------------ 2. 检查是否需要发送 ------------------
        auto now = std::chrono::steady_clock::now();
        if (now - lastSendTime >= sendInterval) {
            cout << ">>> 准备发送数据包..." << endl;
            int status = session->SendPacket(frame.buf.data(), frame.size,
                                             96,       // Payload Type for G.711 PCMA
                                             false,    // Marker bit (false for audio frames usually)
                                             20*8);    // Timestamp increment (20ms * 8kHz)
            CHECK_ERROR(status);
            // cout << "[发送] RTP包，大小=" << frame.size << " bytes" << endl;
            lastSendTime = now;
        }

        // ------------------ 3. 短暂休眠，避免CPU空转 ------------------
        // 休眠时间应该远小于发送间隔
        // std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // 在实际应用中，running会由其他逻辑控制（如用户输入、信号等）
        // 这里为了演示，我们让它运行一小段时间
        // running = ...;
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
    // sessionparams.SetUsePollThread(true);         // 另一种选择：使用库的内置轮询线程

    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(localPort);

    int status = session.Create(sessionparams, &transparams);
    CHECK_ERROR(status);

    uint32_t destip = inet_addr(destIP.c_str());
    if (destip == INADDR_NONE) {
        cerr << "无效的目标IP地址" << endl;
        return -1;
    }
    destip = ntohl(destip); // JRTPLIB 要求主机字节序
    status = session.AddDestination(RTPIPv4Address(destip, destPort));
    CHECK_ERROR(status);

    // 伪造音频帧
    MediaFrame frame;
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.size = 160;                   // 20ms @ 8kHz, 8 bits/sample = 160 bytes
    frame.buf.resize(frame.size, 0xAB); // 填充假数据

    cout << "RTP会话已启动，目标: " << destIP << ":" << destPort
         << "，本地监听端口: " << localPort << endl;

    // 启动主处理循环（替换原来的线程）
    processRTP(&session, frame);

    // 循环结束后...
    session.BYEDestroy(RTPTime(10, 0), "Session ended", strlen("Session ended"));
    cout << "RTP会话结束。" << endl;

    return 0;
}