#include <iostream>
#include <thread>
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpsessionparams.h>
#include <portaudio.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace jrtplib;

#define SAMPLE_RATE 8000
#define FRAMES_PER_BUFFER 160
#define LOCAL_PORT 9000
#define REMOTE_PORT 9001
#define REMOTE_IP "127.0.0.1"

RTPSession session;
PaStream *inputStream;
PaStream *outputStream;
uint32_t timestamp = 0;

void setup_rtp(int port, int remotePort) {
    RTPSessionParams sessparams;
    sessparams.SetOwnTimestampUnit(1.0 / SAMPLE_RATE);
    sessparams.SetAcceptOwnPackets(true);

    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(port);

    int status = session.Create(sessparams, &transparams);
    if (status < 0) {
        // std::cerr << "Error creating RTP session: " << session.GetErrorString(status) << std::endl;
        exit(1);
    }

    uint32_t ip = ntohl(inet_addr(REMOTE_IP));
    session.AddDestination(RTPIPv4Address(ip, remotePort));
}

void audio_send() {
    int16_t buffer[FRAMES_PER_BUFFER];
    while (true) {
        Pa_ReadStream(inputStream, buffer, FRAMES_PER_BUFFER);
        session.SendPacket(buffer, FRAMES_PER_BUFFER * sizeof(int16_t), 0, false, timestamp);
        timestamp += FRAMES_PER_BUFFER;
        usleep(20000);
    }
}

void audio_receive() {
    while (true) {
        session.Poll();
        if (session.GotoFirstSourceWithData()) {
            do {
                RTPPacket *packet;
                while ((packet = session.GetNextPacket()) != nullptr) {
                    int frames = packet->GetPayloadLength() / sizeof(int16_t);
                    Pa_WriteStream(outputStream, packet->GetPayloadData(), frames);
                    session.DeletePacket(packet);
                }
            } while (session.GotoNextSourceWithData());
        }
        usleep(10000);
    }
}

int main() {
    Pa_Initialize();

    // 打开音频输入
    Pa_OpenDefaultStream(&inputStream, 1, 0, paInt16,
                         SAMPLE_RATE, FRAMES_PER_BUFFER, nullptr, nullptr);
    Pa_StartStream(inputStream);

    // 打开音频输出
    Pa_OpenDefaultStream(&outputStream, 0, 1, paInt16,
                         SAMPLE_RATE, FRAMES_PER_BUFFER, nullptr, nullptr);
    Pa_StartStream(outputStream);

    setup_rtp(LOCAL_PORT, REMOTE_PORT);

    std::thread sendThread(audio_send);
    std::thread recvThread(audio_receive);

    std::cout << "双向语音通话启动 (本地端口: " << LOCAL_PORT << " 对端端口: " << REMOTE_PORT << ")..." << std::endl;

    sendThread.join();
    recvThread.join();

    Pa_StopStream(inputStream);
    Pa_CloseStream(inputStream);
    Pa_StopStream(outputStream);
    Pa_CloseStream(outputStream);
    Pa_Terminate();

    session.BYEDestroy(RTPTime(10, 0), "Session ended", 14);

    return 0;
}
