# Vandy IEEE SoutheastCon 2026 - Robot Code

This repository contains the code for the autonomous robot.

## Hardware Architecture
- **Raspberry Pi 4**: Main brain, handles high-level logic and vision.
- **ESP32**: Motor controller, ensures smooth movement and safety.
- **Pixy2**: Vision sensor.

## Project Structure
- `RaspberryPi/`: Main python codebase.
    - `robot_controller.py`: The entry point.
    - `core/`: State machine base classes.
    - `states/`: Robot behavior states.
    - `subsystems/`: Hardware drivers (DriveTrain, Vision).
- `ESP/`: C++ firmware for the motor controller.

## Development Setup (Python)

To ensure everyone uses the same dependencies, please use a virtual environment.

### 1. Prerequisites
- Python 3.9+ installed.

### 2. Create Virtual Environment
Run this in the root of the project:

**Mac/Linux:**
```bash
python3 -m venv venv
source venv/bin/activate
```

**Windows:**
```powershell
python -m venv venv
.\venv\Scripts\Activate
```

### 3. Install Dependencies
```bash
pip install -r requirements.txt
```

### 4. Running the Code
To run the robot controller (or simulations):
```bash
python3 RaspberryPi/robot_controller.py
```
To run tests:
```bash
python3 RaspberryPi/test_refactoring.py
```
