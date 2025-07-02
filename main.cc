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
#include <csignal>

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

// ----------------------- Global Control -----------------------
// 使用 atomic bool 来安全地从主线程停止工作线程
std::atomic<bool> running {true};

// 信号处理函数，用于优雅地停止程序 (e.g., Ctrl+C)
void signalHandler(int signum) {
    cout << "\n捕获到中断信号，准备退出..." << endl;
    running = false;
}

// ----------------------- Threads -----------------------

/**
 * @brief 发送线程.
 * 
 * @param session 指向 RTPSession 对象的指针.
 * @param frame 要发送的媒体帧.
 */
void senderThread(RTPSession *session, MediaFrame frame)
{
    // std::cout << __FUNCTION__ << std::endl;
    // 模拟20ms发送一次音频包
    const auto sendInterval = std::chrono::milliseconds(20);
    // 时间戳增量: 20ms * 8kHz = 160
    const uint32_t timestampIncrement = 160; 
    // 负载类型: 96 是一个常见的动态类型
    const uint8_t payloadType = 96;

    while (running) {
        // JRTPLIB 的内部锁会确保 SendPacket 是线程安全的
        int status = session->SendPacket(frame.buf.data(), frame.size, payloadType, false, timestampIncrement);
        
        if (status < 0) {
             cerr << "发送失败: " << RTPGetErrorString(status) << endl;
        } else {
             cout << ">>> 发送 RTP 包, 大小=" << frame.size << endl;
        }

        std::this_thread::sleep_for(sendInterval);
    }
    cout << "发送线程已停止。" << endl;
}

/**
 * @brief 接收线程.
 * 
 * @param session 指向 RTPSession 对象的指针.
 */
void receiverThread(RTPSession *session)
{
    while (running) {
        // *** 注意：这里不再需要调用 session->Poll() ***
        // JRTPLIB 的后台轮询线程正在为我们做这件事。

        std::cout << __FUNCTION__ << std::endl;
        session->BeginDataAccess();

        // 检查是否有新的数据源
        if (session->GotoFirstSourceWithData()) {
            do {
                RTPPacket *pack;
                // 从当前源获取所有数据包
                while ((pack = session->GetNextPacket()) != nullptr) {
                    cout << "[接收] RTP包, 来自 SSRC " << pack->GetSSRC()
                         << ", 大小=" << pack->GetPayloadLength()
                         << " bytes, 序列号=" << pack->GetSequenceNumber() << endl;
                    
                    // 使用完数据包后必须删除它
                    session->DeletePacket(pack);
                }
            } while (session->GotoNextSourceWithData());
        }

        session->EndDataAccess();

        // 休眠一小段时间，避免CPU空转，同时保持响应性
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    cout << "接收线程已停止。" << endl;
}

// ----------------------- RTP Setup & Main -----------------------
int main(int argc, char *argv[])
{
    if (argc != 4) {
        std::cerr << "用法: " << argv[0] << " <目标IP> <目标端口> <本地端口>" << std::endl;
        std::cerr << "示例: " << argv[0] << " 127.0.0.1 6000 5000" << std::endl;
        return -1;
    }

    // 注册信号处理，以便按 Ctrl+C 可以优雅退出
    signal(SIGINT, signalHandler);

    std::string destIP = argv[1];
    uint16_t destPort = static_cast<uint16_t>(std::stoi(argv[2]));
    uint16_t localPort = static_cast<uint16_t>(std::stoi(argv[3]));

    // 1. RTP 会话对象
    RTPSession session;

    // 2. 配置会话参数
    RTPSessionParams sessionparams;
    // 设置时间戳单位。对于8kHz音频，每个采样点是 1/8000 秒。
    sessionparams.SetOwnTimestampUnit(1.0 / 8000.0); 
    // 允许接收自己发送的数据包，方便本地回环测试
    sessionparams.SetAcceptOwnPackets(true);
    // *** 关键设置：启用 JRTPLIB 的内置轮询线程 ***
    sessionparams.SetUsePollThread(true);

    // 3. 配置传输参数
    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(localPort);

    // 4. 创建会话
    int status = session.Create(sessionparams, &transparams);
    CHECK_ERROR(status);

    // 5. 添加目标地址
    uint32_t destip = inet_addr(destIP.c_str());
    if (destip == INADDR_NONE) {
        cerr << "无效的目标IP地址: " << destIP << endl;
        return -1;
    }
    destip = ntohl(destip); // JRTPLIB 要求主机字节序
    status = session.AddDestination(RTPIPv4Address(destip, destPort));
    CHECK_ERROR(status);

    // 伪造一个音频帧用于发送
    MediaFrame frame;
    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.size = 160;                   // 20ms @ G.711 (8kHz, 8bit) = 160 bytes
    frame.buf.resize(frame.size, 0xAB); // 用0xAB填充数据

    cout << "RTP会话已启动，目标: " << destIP << ":" << destPort
         << "，本地监听端口: " << localPort << endl;
    cout << "按 Ctrl+C 退出程序。" << endl;

    // 6. 启动工作线程
    std::thread recvThread(receiverThread, &session);
    std::thread sendThread(senderThread, &session, frame);

    // 主线程等待，直到 running 变为 false (例如通过 Ctrl+C)
    while (running) {
        // 可以打印一些RTCP统计信息
        // cout << "源数量: " << session.GetSourceCount() << endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // 7. 停止并清理
    recvThread.join();
    sendThread.join();

    // 发送 BYE 包并销毁会话
    session.BYEDestroy(RTPTime(10, 0), "Session ended", strlen("Session ended"));
    cout << "RTP会话已结束。" << endl;

    return 0;
}