from pathlib import Path
from urllib.request import urlopen
import zipfile
import io

ROOT = Path(__file__).resolve().parent
MODEL_NAME = 'vosk-model-small-en-us-0.15'

def main():
    destination = ROOT / 'models'
    destination.mkdir(exist_ok=True)
    if (destination / MODEL_NAME / 'am/final.mdl').exists():
        print('Offline speech model ready.')
        return
    print('Downloading offline English speech model ...', flush=True)
    with urlopen(f'https://alphacephei.com/vosk/models/{MODEL_NAME}.zip', timeout=120) as response:
        content = response.read()
    with zipfile.ZipFile(io.BytesIO(content)) as archive:
        for item in archive.infolist():
            target = (destination / item.filename).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise ValueError('Unsafe archive path')
        archive.extractall(destination)
    print('Offline speech model ready.')

if __name__ == '__main__':
    main()
