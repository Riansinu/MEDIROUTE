# MediRoute dashboard companion

Install the dependencies and models below. Then run `Start-MediRoute.cmd` and open **http://localhost:8765** in Chrome or Edge. Keep the terminal open. Run the app on the same computer as the browser.

If the embedded preview cannot access the camera, open `Open-Dashboard.cmd` to load the same address in your default browser. Use Chrome or Edge, click Enable camera and allow camera access. The page reports the exact browser error if it fails. `NotAllowedError` indicates a permission or policy block; `NotFoundError` means no camera was found; `NotReadableError` means the camera could not start. Check Windows camera permissions or other camera apps as appropriate. Recovery login is available only after first-time enrolment succeeds.

## First use

1. Copy the first-time setup code printed in the server terminal.
2. Enter your name, that code and a recovery password (12+ characters).
3. Agree to local template storage, enable the camera and select **Enrol & enter**.
4. On subsequent visits choose **Verify & enter**, or use the recovery password.
5. Use **Ask MediRoute** on the right. Speak starts microphone recording; Stop transcribes locally for you to review; Ask submits it. Recording stops automatically after 30 seconds. Read aloud and Stop audio control playback.

From the `mediroute-dashboard` directory, install the dependencies and models with Python 3.14.

```powershell
python -m pip install --target vendor -r requirements.txt
python setup_models.py
python setup_speech.py
python app.py
```

## What was implemented

- Camera preview, local face enrolment and server-side face matching using OpenCV YuNet and SFace.
- One owner profile, password recovery login, expiring signed sessions, login rate limiting, same-origin POST checks and authenticated dashboard/API routes.
- Offline microphone transcription, typed and spoken project questions, firmware-grounded knowledge retrieval, spoken responses, sources, stop-audio and browser-error handling.
- Optional local generative AI via Ollama, with automatic fallback to curated answers.
- Your original Arduino dashboard served in an authenticated frame, with same-origin proxy endpoints for connection to the ESP32.
- Account deletion using the recovery password. No raw camera images or conversations are saved by the application.

The companion reads the dashboard from the `.ino` file at the repository root. You can use face login and voice input without uploading firmware.

## Connect the original dashboard

Connect the laptop to `MediRoute_Robot` Wi-Fi, then select **Connect dashboard**. The server defaults to the robot at `http://192.168.4.1`. Override before starting if needed:

```powershell
$env:ESP32_URL = 'http://192.168.4.1'
python app.py
```

The project assistant and face verification do not require the robot. The original firmware still exposes its own unprotected dashboard and control endpoints; this companion login protects only this app, not direct access to the ESP32. The frame sends commands only when its delivery buttons are clicked.

## Voice and AI

Default **Project knowledge** mode uses curated retrieval, not a generative model. Edit `knowledge.json` or `knowledge_extra.json` to extend its coverage. The assistant now covers 62 topics, with weighted question matching, spelling tolerance, source references, related questions and a topic picker. Knowledge edits reload automatically without restarting. It handles known project topics and explicitly falls back when no documented topic matches. Microphone audio is captured using Web Audio and transcribed locally with Vosk; voice input, typed answers and face matching work offline after installation. The microphone-level indicator shows whether audio is being captured. Audio is not saved. Available speech synthesis voices depend on the device.

To use an already installed Ollama model, start Ollama locally and set its exact model name before launching:

```powershell
$env:OLLAMA_MODEL = 'your-installed-model-name'
python app.py
```

The app calls Ollama only at `127.0.0.1:11434` and provides the curated project context. No paid API key is needed. Generated answers can be wrong; the assistant never executes robot commands.

## Face verification limits and data

This is a functional academic prototype, not production-grade biometric authentication. It compares face embeddings on the server at a cosine threshold of 0.45. It has **no certified liveness/anti-spoofing detection**: photographs or replayed video may pass. Lighting, camera quality and facial variation can cause false rejection or acceptance. Evaluate on intended users before relying on it. Use the recovery password when matching fails.

`data/profile.json` stores the name, password hash and face template locally. Templates are biometric data, stored unencrypted on disk; protect the Windows account and folder. Account deletion removes that file. Frames are processed in memory without application-level image storage. The setup code is printed in the terminal and initial enrolment closes after the first profile is created. Sessions expire after two hours and server restarts invalidate them. The server listens only on loopback; remote hosting requires additional deployment and security work.

## Models and attribution

Models are downloaded from the official [OpenCV Zoo](https://github.com/opencv/opencv_zoo). YuNet: `face_detection_yunet_2026may.onnx`; SFace: `face_recognition_sface_2021dec.onnx`. See each upstream model directory for its license. `models/download-sha256.json` records the downloaded file hashes. Matching follows the [OpenCV face recognition API](https://docs.opencv.org/4.12.0/d0/dd4/tutorial_dnn_face.html).

## Verification

Run `python -m unittest discover -s tests -v`. Automated tests cover session protection, origin checks, enrolment closure, face-match decisions, password login, throttling, profile deletion, blank-image rejection, assistant fallback and protected firmware rendering. A real user must still verify camera access, face enrolment/matching and speech recognition on their device. Robot hardware was not exercised.

The optional recording test uses the [Vosk example WAV](https://github.com/alphacep/vosk-api/blob/master/python/example/test.wav). Save it as `tests/speech-sample.wav` to enable that test. Run `node --test tests/audio.test.cjs` to check audio capture and conversion.

Speech model: [Vosk small English US 0.15](https://alphacephei.com/vosk/models), Apache 2.0. Recognition accuracy varies with accent, noise and technical vocabulary; review the transcript before asking.
