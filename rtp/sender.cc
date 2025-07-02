#include <iostream>
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpsessionparams.h>
#include <portaudio.h>
#include <unistd.h>  // usleep
#include <arpa/inet.h>

using namespace jrtplib;

#define SAMPLE_RATE 8000
#define FRAMES_PER_BUFFER 160
#define PORT_BASE 9000
#define DEST_IP "127.0.0.1"
#define DEST_PORT 9002

int main() {
    Pa_Initialize();

    PaStream *inputStream;
    Pa_OpenDefaultStream(&inputStream, 1, 0, paInt16,
                         SAMPLE_RATE, FRAMES_PER_BUFFER, NULL, NULL);
    Pa_StartStream(inputStream);

    // RTP 初始化
    RTPSession sess;
    RTPSessionParams sessparams;
    sessparams.SetOwnTimestampUnit(1.0 / SAMPLE_RATE);
    sessparams.SetAcceptOwnPackets(true);
    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(PORT_BASE);

    sess.Create(sessparams, &transparams);

    uint32_t ip = inet_addr(DEST_IP);
    ip = ntohl(ip);
    sess.AddDestination(RTPIPv4Address(ip, DEST_PORT));

    int16_t buffer[FRAMES_PER_BUFFER];

    std::cout << "Sending audio..." << std::endl;
    while (true) {
        Pa_ReadStream(inputStream, buffer, FRAMES_PER_BUFFER);
        sess.SendPacket((void *)buffer, FRAMES_PER_BUFFER * sizeof(int16_t));
        usleep(20000);
    }

    Pa_StopStream(inputStream);
    Pa_CloseStream(inputStream);
    Pa_Terminate();
    sess.BYEDestroy(RTPTime(10,0), "Session ended", 14);

    return 0;
}
