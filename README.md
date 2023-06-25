# Coral Dev Board Micro Pipeline Leak Detection

## Get the project locally

1. Clone `pipeline-leak-detection` and all submodules:

    ```bash
    git clone --recurse-submodules -j8 https://github.com/having11/pipeline-leak-detection
    ```

2. Install the required tools:

    ```bash
    cd pipeline-leak-detection && bash setup.sh
    ```

## Build the code

This builds everything in a folder called `build` (or you can specify a
different path with `-b`, but if you do then you must also specify that path
everytime you call `flashtool.py`):

```bash
bash build.sh
```

## Flash the board

```bash
python3 scripts/flashtool.py --app pipeline_detection --wifi_ssid <YOUR_SSID> --wifi_psk <YOUR_PSK>
```

### Reset the board to Serial Downloader

Flashing the Dev Board Micro might fail sometimes and you can usually solve
it by starting Serial Downloader mode in one of two ways:

+ Hold the User button while you press the Reset button.
+ Or, hold the User button while you plug in the USB cable.

Then try flashing the board again.

For more details, see the [troubleshooting info on
coral.ai](https://coral.ai/docs/dev-board-micro/get-started/#serial-downloader).
