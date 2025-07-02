import socket
import struct
import numpy as np
import time

def generate_sine_wave(freq, duration, rate):
    t = np.linspace(0, duration, int(rate * duration), endpoint=False)
    audio = 0.5 * np.sin(2 * np.pi * freq * t)
    return (audio * 32767).astype(np.int16)

def send_rtp(audio_data, ip='127.0.0.1', port=5004):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    seq = 0
    timestamp = 0
    payload_type = 96  # dynamic

    for i in range(0, len(audio_data), 160):  # 每20ms发送一次，160 samples @ 8000 Hz
        chunk = audio_data[i:i+160].tobytes()
        header = struct.pack("!BBHII",
                             0x80, payload_type,
                             seq, timestamp, 0x12345678)
        packet = header + chunk
        sock.sendto(packet, (ip, port))
        seq += 1
        timestamp += 160
        time.sleep(0.02)

SAMPLE_RATE = 8000      # 采样率
FREQ = 440              # 正弦波频率（Hz）
DURATION = 5            # 持续时间（秒）
RTP_PORT = 5004         # 端口号

if __name__ == "__main__":
    sine = generate_sine_wave(FREQ, DURATION, SAMPLE_RATE)
    send_rtp(sine)
