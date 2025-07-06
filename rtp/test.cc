#include <iostream>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <arpa/inet.h>

#include <portaudio.h>
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpsessionparams.h>

using namespace jrtplib;

#define SAMPLE_RATE 8000
#define FRAMES_PER_BUFFER 160
#define SEND_PORT 9000
#define RECV_PORT 9002
#define DEST_IP "127.0.0.1"

std::atomic<bool> running(true);

void senderThread(PaStream *inputStream) {
    RTPSession sess;
    RTPSessionParams sessparams;
    sessparams.SetOwnTimestampUnit(1.0 / SAMPLE_RATE);
    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(SEND_PORT);

    if (sess.Create(sessparams, &transparams) < 0) {
        std::cerr << "Error creating RTP session (sender)" << std::endl;
        return;
    }

    uint32_t ip = ntohl(inet_addr(DEST_IP));
    sess.AddDestination(RTPIPv4Address(ip, RECV_PORT));

    int16_t buffer[FRAMES_PER_BUFFER];

    while (running) {
        Pa_ReadStream(inputStream, buffer, FRAMES_PER_BUFFER);
        sess.SendPacket((void *)buffer, FRAMES_PER_BUFFER * sizeof(int16_t));
        usleep(20000);  // 20ms
    }

    sess.BYEDestroy(RTPTime(1, 0), "Bye", 3);
}

void receiverThread(PaStream *outputStream) {
    RTPSession sess;
    RTPSessionParams sessparams;
    sessparams.SetOwnTimestampUnit(1.0 / SAMPLE_RATE);
    sessparams.SetAcceptOwnPackets(true);  // Just in case
    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(RECV_PORT);

    if (sess.Create(sessparams, &transparams) < 0) {
        std::cerr << "Error creating RTP session (receiver)" << std::endl;
        return;
    }

    while (running) {
        sess.Poll();
        if (sess.GotoFirstSourceWithData()) {
            do {
                RTPPacket *packet;
                while ((packet = sess.GetNextPacket()) != NULL) {
                    Pa_WriteStream(outputStream, packet->GetPayloadData(), FRAMES_PER_BUFFER);
                    sess.DeletePacket(packet);
                }
            } while (sess.GotoNextSourceWithData());
        }
        usleep(5000);
    }

    sess.BYEDestroy(RTPTime(1, 0), "Bye", 3);
}

int main() {
    Pa_Initialize();

    // 输入设备（麦克风）
    PaStream *inputStream;
    Pa_OpenDefaultStream(&inputStream, 1, 0, paInt16,
                         SAMPLE_RATE, FRAMES_PER_BUFFER, NULL, NULL);
    Pa_StartStream(inputStream);

    // 输出设备（扬声器）
    PaStream *outputStream;
    Pa_OpenDefaultStream(&outputStream, 0, 1, paInt16,
                         SAMPLE_RATE, FRAMES_PER_BUFFER, NULL, NULL);
    Pa_StartStream(outputStream);

    std::thread sender(senderThread, inputStream);
    std::thread receiver(receiverThread, outputStream);

    std::cout << "Running... Press Enter to stop." << std::endl;
    std::cin.get();
    running = false;

    sender.join();
    receiver.join();

    Pa_StopStream(inputStream);
    Pa_CloseStream(inputStream);
    Pa_StopStream(outputStream);
    Pa_CloseStream(outputStream);
    Pa_Terminate();

    return 0;
}
