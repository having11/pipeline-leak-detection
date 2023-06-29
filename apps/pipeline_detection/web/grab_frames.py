from PIL import Image
import requests
import sys
import os, os.path as path
import time

URL = f'http://192.168.0.17/camera_stream'
DEST_FOLDER = f'captures'
FPS = 10
DELAY_SECONDS = 1 / FPS
TIMEOUT_SECONDS = 3

homepath = path.expanduser('~')
full_path = path.join(homepath, DEST_FOLDER)
print(f'Using path={full_path} to store captures')
if not path.exists(full_path):
    os.mkdir(full_path)

while True:
    try:
        print('Attempting to grab image...')
        im = Image.open(requests.get(URL, stream=True, timeout=TIMEOUT_SECONDS).raw)
        filename = f'capture{int(time.time)}.jpg'
        print(f'Saving image={filename}')
        im.save(path.join(full_path, filename))
        time.sleep(DELAY_SECONDS)
        
    except KeyboardInterrupt:
        print('\nKeyboard interrupt. Exiting...')
        sys.exit()
        
    except Exception as err:
        print('Error getting image:', err)