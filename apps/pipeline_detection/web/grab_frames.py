from PIL import Image
import requests
import sys
from io import BytesIO
import os, os.path as path
import time
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('ip_address', help='Coral micro\'s IP address', metavar='IP_ADDRESS')
parser.add_argument('-o', '--output', help='Folder to store captured images')
parser.add_argument('--fps', help='Capture images at this rate per second', type=int, default=5)
parser.add_argument('--prefix', help='Prefix the image with this string; helpful\
for categorizing image labels for ML', default='')
parser.add_argument('--timeout', help='Timeout after waiting this many seconds for an image',
                    type=int, default=3)
parser.add_argument('--rotate', help='Rotate captures by this many degrees', type=int,
                    default=0)
parser.add_argument('--resize', help='Resize to given size', metavar='WIDTH HEIGHT',
                    type=int, nargs=2)
args = parser.parse_args()

URL = f'http://{args.ip_address}/camera_stream'
DEST_FOLDER = f'captures'
DELAY_SECONDS = 1 / args.fps
if args.prefix != '':
    args.prefix += '_'

homepath = path.expanduser('~')
full_path = args.output if args.output is not None else path.join(homepath, DEST_FOLDER)
print(f'Using path={full_path} to store captures')
if not path.exists(full_path):
    os.mkdir(full_path)

while True:
    try:
        print('Attempting to grab image...')
        im = Image.open(BytesIO(requests.get(URL, stream=True, timeout=args.timeout).content))
        im = im.rotate(args.rotate)
        if args.resize:
            im = im.resize(args.resize)
        filename = f'{args.prefix}capture{time.time():.2f}.jpg'
        print(f'Saving image={filename}')
        im.save(path.join(full_path, filename))
        time.sleep(DELAY_SECONDS)
        
    except KeyboardInterrupt:
        print('\nKeyboard interrupt. Exiting...')
        sys.exit()
        
    except Exception as err:
        print('Error getting image:', err)