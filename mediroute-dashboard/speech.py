import base64
import json
import threading
from pathlib import Path

MODEL_PATH = Path(__file__).resolve().parent / 'models/vosk-model-small-en-us-0.15'
_model = None
_lock = threading.Lock()

def transcribe(encoded):
    if not isinstance(encoded, str):
        raise ValueError('Audio must be base64 PCM.')
    try:
        pcm = base64.b64decode(encoded, validate=True)
    except Exception:
        raise ValueError('Invalid audio encoding.')
    if len(pcm) < 3200 or len(pcm) > 960000 or len(pcm) % 2:
        raise ValueError('Record between 0.1 and 30 seconds of audio.')
    if not MODEL_PATH.exists():
        raise RuntimeError('Speech model missing. Run python setup_speech.py.')
    global _model
    from vosk import Model, KaldiRecognizer, SetLogLevel
    with _lock:
        if _model is None:
            SetLogLevel(-1)
            _model = Model(str(MODEL_PATH))
    recognizer = KaldiRecognizer(_model, 16000)
    parts = []
    for position in range(0, len(pcm), 8000):
        if recognizer.AcceptWaveform(pcm[position:position+8000]):
            parts.append(json.loads(recognizer.Result()).get('text', ''))
    parts.append(json.loads(recognizer.FinalResult()).get('text', ''))
    return ' '.join(part for part in parts if part).strip()
