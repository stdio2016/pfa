import wave
import numpy as np

wav = wave.open('adversary.wav', 'wb')
wav.setsampwidth(2)
wav.setframerate(8820)
wav.setnchannels(1)

bad = np.zeros(8820*10, dtype=np.int16)
bad[256::512] = 32767
wav.writeframes(bad.tobytes())
