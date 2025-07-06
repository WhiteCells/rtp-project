#include <iostream>
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
#define PORT_BASE 9000
#define DEST_IP "192.168.240.192"
#define DEST_PORT 9000  // 要与 receiver 的 PORT_BASE 一致

int main() {
    Pa_Initialize();

    PaStream *inputStream;
    PaError err = Pa_OpenDefaultStream(&inputStream, 1, 0, paInt16,
                                       SAMPLE_RATE, FRAMES_PER_BUFFER, nullptr, nullptr);
    if (err != paNoError) {
        std::cerr << "Failed to open input stream: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    Pa_StartStream(inputStream);

    // RTP 初始化
    RTPSession sess;
    RTPSessionParams sessparams;
    sessparams.SetOwnTimestampUnit(1.0 / SAMPLE_RATE);
    sessparams.SetAcceptOwnPackets(true);
    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(PORT_BASE);

    int status = sess.Create(sessparams, &transparams);
    if (status < 0) {
        std::cerr << "Error creating RTP session!" << std::endl;
        return 1;
    }

    uint32_t ip = inet_addr(DEST_IP);
    ip = ntohl(ip); // JRTPLib 使用主机字节序
    sess.AddDestination(RTPIPv4Address(ip, DEST_PORT));

    int16_t buffer[FRAMES_PER_BUFFER];
    uint32_t timestamp = 0;

    std::cout << "Sending audio to " << DEST_IP << ":" << DEST_PORT << "..." << std::endl;

    while (true) {
        err = Pa_ReadStream(inputStream, buffer, FRAMES_PER_BUFFER);
        if (err != paNoError) {
            std::cerr << "Error reading from input: " << Pa_GetErrorText(err) << std::endl;
            break;
        }

        sess.SendPacket(buffer, FRAMES_PER_BUFFER * sizeof(int16_t), 0, false, timestamp);
        timestamp += FRAMES_PER_BUFFER;

        usleep(20000);  // 控制帧率，适配 8000Hz 采样率
    }

    Pa_StopStream(inputStream);
    Pa_CloseStream(inputStream);
    Pa_Terminate();
    sess.BYEDestroy(RTPTime(10, 0), "Session ended", 14);

    return 0;
}
