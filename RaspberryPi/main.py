import logging
from robot_controller import RobotController

def main():
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    
    print("Starting Robot Software...")
    robot = RobotController()
    robot.run()

if __name__ == '__main__':
    main()
