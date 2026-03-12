# Raspberry Pi Setup Guide

This guide contains all the necessary commands and steps to set up the software environment for the robot on a newly flashed Raspberry Pi.

## 1. System Updates and Dependencies

First, ensure your Raspberry Pi system is up-to-date and install the required system libraries for building dependencies (like Pixy2's Python wrappers).

```bash
sudo apt update
sudo apt upgrade -y
sudo apt install -y build-essential swig libusb-1.0-0-dev python3-dev python3-venv git
```

## 2. Clone Gitignored Repositories

Because certain third-party dependencies are intentionally ignored in `.gitignore`, you will need to clone them into the root of this repository before proceeding to build them.

```bash
# From the root of this repository (Vandy_IEEE_SoutheastCon_2026/):
git clone https://github.com/charmedlabs/pixy2.git
```

## 3. Raspberry Pi Configuration (SPI & Serial)

The robot requires SPI for communication (e.g., `spidev` in `spi_test.py`) and Serial (UART) for communicating with the ESP32 via `pyserial` (`drivetrain.py`).

Run the Raspberry Pi configuration tool:
```bash
sudo raspi-config
```
1. Go to **Interface Options**.
2. **SPI**: Select `Yes` to enable the SPI interface.
3. **Serial Port**: Select `No` for login shell over serial, and `Yes` to enable the serial port hardware.
4. Reboot the Raspberry Pi if prompted.

Alternatively, enable them non-interactively via the command line:
```bash
sudo raspi-config nonint do_spi 0
sudo raspi-config nonint do_serial_hw 0
sudo raspi-config nonint do_serial_cons 1
```

## 4. Install pigpio Daemon

The robot uses `pigpio` for low-level hardware control. Although the project has a `pigpio-master` folder locally, you can install the daemon directly via `apt` or build it from the included source.

**Option A (Recommended): Install via apt**
```bash
sudo apt install -y pigpio
```

**Option B: Build from local source**
```bash
cd RaspberryPi/subsystems
unzip master.zip
cd pigpio-master
make
sudo make install
```

**Enable and start the pigpio daemon:**
```bash
sudo systemctl enable pigpiod
sudo systemctl start pigpiod
```

## 5. Python Virtual Environment

It is recommended to use a virtual environment to manage the Python dependencies. Run this from the root folder of the repository (`Vandy_IEEE_SoutheastCon_2026/`):

```bash
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip setuptools
pip install -r requirements.txt
```

## 6. Build Pixy2 Libraries and Python Bindings

The `pixy_detect.py` subsystem depends on the Pixy2 Python wrappers. You must compile the USB library and SWIG bindings first.

1. **Build the libpixyusb2 library:**
```bash
cd pixy2/scripts
./build_libpixyusb2.sh
```

2. **Build the Python wrappers:**
```bash
./build_python_demos.sh
```

*(Note: Ensure your `PYTHONPATH` includes the directory containing the generated `pixy.py` and `_pixy.so` files—located at `pixy2/build/python_demos`—or copy those files directly into the `RaspberryPi/subsystems/vision/` directory so they can be imported successfully).*

## 7. Running the Code

Once the setup is complete, you can start the robot controller:

```bash
# Don't forget to activate the virtual environment if not already active
source venv/bin/activate

# Return to repo root if in a subfolder
cd /path/to/Vandy_IEEE_SoutheastCon_2026

python3 RaspberryPi/robot_controller.py
```
