#include <portaudio.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <mutex>

#define SAMPLE_RATE       8000
#define FRAMES_PER_BUFFER 160
#define NUM_CHANNELS      1
#define SAMPLE_FORMAT     paInt16

using SampleType = int16_t;

std::ofstream pcmOutFile;
std::mutex fileMutex;

float sinePhase = 0.0f;
const float TWO_PI = 2 * M_PI;
const float FREQUENCY = 440.0f;

// 麦克风录音回调，写入文件
static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    const SampleType *input = static_cast<const SampleType *>(inputBuffer);
    if (!input) {
        return paContinue;
    }

    std::lock_guard<std::mutex> lock(fileMutex);
    std::cout << "frames pre buffer: " << framesPerBuffer << std::endl;
    std::cout << "input: " << reinterpret_cast<const char *>(input) << std::endl;
    pcmOutFile.write(reinterpret_cast<const char *>(input), framesPerBuffer * sizeof(SampleType));

    return paContinue;
}

// 生成 440Hz 正弦波到扬声器
static int playCallback(const void *inputBuffer, void *outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo *timeInfo,
                        PaStreamCallbackFlags statusFlags,
                        void *userData)
{
    SampleType *output = static_cast<SampleType *>(outputBuffer);

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float sample = std::sin(sinePhase) * 0.3f;           // 降低振幅避免破音
        output[i] = static_cast<SampleType>(sample * 32767); // 转换为 16-bit PCM

        sinePhase += TWO_PI * FREQUENCY / SAMPLE_RATE;
        if (sinePhase >= TWO_PI)
            sinePhase -= TWO_PI;
    }

    return paContinue;
}

int main()
{
    Pa_Initialize();

    // 打开 PCM 输出文件
    pcmOutFile.open("output.pcm", std::ios::binary);
    if (!pcmOutFile) {
        std::cerr << "failed open output.pcm\n";
        return 1;
    }

    // --- 设置录音设备参数 ---
    PaStream *recordStream;
    PaStreamParameters inputParams;
    inputParams.device = Pa_GetDefaultInputDevice();
    inputParams.channelCount = NUM_CHANNELS;
    inputParams.sampleFormat = SAMPLE_FORMAT;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    Pa_OpenStream(&recordStream, &inputParams, nullptr,
                  SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff,
                  recordCallback, nullptr);

    // --- 设置播放设备参数 ---
    PaStream *playStream;
    PaStreamParameters outputParams;
    outputParams.device = Pa_GetDefaultOutputDevice();
    outputParams.channelCount = NUM_CHANNELS;
    outputParams.sampleFormat = SAMPLE_FORMAT;
    outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    Pa_OpenStream(&playStream, nullptr, &outputParams,
                  SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff,
                  playCallback, nullptr);

    // 启动录音与播放
    Pa_StartStream(recordStream);
    Pa_StartStream(playStream);

    std::cout << "record mic_output.pcm，play 440Hz audio... press Enter to stop\n";
    std::cin.get();

    Pa_StopStream(recordStream);
    Pa_StopStream(playStream);
    Pa_CloseStream(recordStream);
    Pa_CloseStream(playStream);
    Pa_Terminate();
    pcmOutFile.close();

    return 0;
}
