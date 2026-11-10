from pathlib import Path
from urllib.request import urlopen
import hashlib
import json

ROOT = Path(__file__).resolve().parent
MODELS = {
    'yunet.onnx': ('face_detection_yunet', 'face_detection_yunet_2026may.onnx'),
    'sface.onnx': ('face_recognition_sface', 'face_recognition_sface_2021dec.onnx'),
}

def main():
    folder = ROOT / 'models'
    folder.mkdir(exist_ok=True)
    hashes = {}
    for local, (directory, filename) in MODELS.items():
        target = folder / local
        if not target.exists():
            url = f'https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models/{directory}/{filename}'
            print(f'Downloading {filename} ...')
            with urlopen(url, timeout=120) as response:
                content = response.read()
            if len(content) < 100000:
                raise RuntimeError('Model download is incomplete')
            target.write_bytes(content)
        hashes[local] = hashlib.sha256(target.read_bytes()).hexdigest()
    (folder / 'download-sha256.json').write_text(json.dumps(hashes, indent=2))
    print('Face models ready.')

if __name__ == '__main__':
    main()
