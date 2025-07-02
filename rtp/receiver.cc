#include <iostream>
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpsessionparams.h>
#include <portaudio.h>
#include <unistd.h>

using namespace jrtplib;

#define SAMPLE_RATE 8000
#define FRAMES_PER_BUFFER 160
#define PORT_BASE 9002

int main() {
    Pa_Initialize();

    PaStream *outputStream;
    Pa_OpenDefaultStream(&outputStream, 0, 1, paInt16,
                         SAMPLE_RATE, FRAMES_PER_BUFFER, NULL, NULL);
    Pa_StartStream(outputStream);

    RTPSession sess;
    RTPSessionParams sessparams;
    sessparams.SetOwnTimestampUnit(1.0 / SAMPLE_RATE);
    sessparams.SetAcceptOwnPackets(true);
    RTPUDPv4TransmissionParams transparams;
    transparams.SetPortbase(PORT_BASE);
    sess.Create(sessparams, &transparams);

    std::cout << "Waiting for audio packets on port " << PORT_BASE << "..." << std::endl;

    while (true) {
        sess.Poll();
        if (sess.GotoFirstSourceWithData()) {
            do {
                RTPPacket *packet;
                while ((packet = sess.GetNextPacket()) != NULL) {
                    Pa_WriteStream(outputStream, packet->GetPayloadData(),
                                   FRAMES_PER_BUFFER);
                    sess.DeletePacket(packet);
                }
            } while (sess.GotoNextSourceWithData());
        }
        usleep(10000);
    }

    Pa_StopStream(outputStream);
    Pa_CloseStream(outputStream);
    Pa_Terminate();
    sess.BYEDestroy(RTPTime(10,0), "Session ended", 14);

    return 0;
}
