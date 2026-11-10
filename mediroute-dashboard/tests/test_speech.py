import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import app
import speech
import unittest
import base64
import wave
from unittest.mock import patch

class SpeechTests(unittest.TestCase):
    def test_invalid_audio_rejected(self):
        for value in [None, '%%%', '', base64.b64encode(b'x'*3201).decode(), base64.b64encode(b'x'*960002).decode()]:
            with self.assertRaises(ValueError):
                speech.transcribe(value)

    def test_silence(self):
        self.assertEqual(speech.transcribe(base64.b64encode(bytes(32000)).decode()), '')

    def test_official_recording(self):
        sample = Path(__file__).with_name('speech-sample.wav')
        if not sample.is_file():
            self.skipTest('Optional Vosk test recording is missing. See README.md.')
        with wave.open(str(sample)) as wav:
            self.assertEqual((wav.getframerate(),wav.getnchannels(),wav.getsampwidth()),(16000,1,2))
            audio=wav.readframes(wav.getnframes())
        result=speech.transcribe(base64.b64encode(audio).decode())
        self.assertIn('one zero zero zero one', result)

    def test_endpoint_auth_and_errors(self):
        client=app.app.test_client()
        options={'base_url':'http://localhost:8765','headers':{'Origin':'http://localhost:8765'}}
        self.assertEqual(client.post('/api/transcribe',json={},**options).status_code,401)
        with client.session_transaction(base_url=options['base_url']) as session:
            session['authenticated']=True
        self.assertEqual(client.post('/api/transcribe',json={'audio':'invalid'},**options).status_code,400)
        with patch.object(speech,'transcribe',return_value='what is the project'):
            self.assertEqual(client.post('/api/transcribe',json={'audio':'test'},**options).json['text'],'what is the project')
        with patch.object(speech,'transcribe',return_value=''):
            self.assertEqual(client.post('/api/transcribe',json={'audio':'test'},**options).status_code,422)

if __name__=='__main__':
    unittest.main()
