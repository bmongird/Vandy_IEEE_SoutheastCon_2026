import argparse
import logging
from robot_controller import RobotController

def main():
    parser = argparse.ArgumentParser(description="Robot Main Application")
    parser.add_argument('--debug', action='store_true', help="Enable debug mode with verbose logging and periodic state output")
    args = parser.parse_args()

    # The test script logs at INFO level. "Regular mode shouldn't log as much as the test does."
    # So we set normal mode to WARNING and debug mode to DEBUG (which includes INFO).
    log_level = logging.DEBUG if args.debug else logging.WARNING
    logging.basicConfig(level=log_level, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    
    print(f"Starting Robot Software... (Debug Mode: {args.debug})")
    robot = RobotController(debug=args.debug)
    robot.run()

if __name__ == '__main__':
    main()
