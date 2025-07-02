import socket
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

BUFFER_SIZE = 2048
RTP_HEADER_SIZE = 12

audio_buffer = []

def receive_rtp(port=5004):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", port))
    print(f"Listening on RTP port {port}...")
    while True:
        data, _ = sock.recvfrom(BUFFER_SIZE)
        payload = data[RTP_HEADER_SIZE:]
        samples = np.frombuffer(payload, dtype=np.int16)
        audio_buffer.extend(samples)

def animate(i):
    plt.cla()
    if len(audio_buffer) > 800:
        plt.plot(audio_buffer[-800:])
        plt.title("Audio Waveform")
        plt.xlabel("Samples")
        plt.ylabel("Amplitude")

if __name__ == "__main__":
    import threading
    t = threading.Thread(target=receive_rtp)
    t.daemon = True
    t.start()

    fig = plt.figure()
    ani = animation.FuncAnimation(fig, animate, interval=50)
    plt.show()
