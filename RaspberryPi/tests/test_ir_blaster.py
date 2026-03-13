import sys
import os
import time

# Add the parent directory to sys.path so we can import from subsystems
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from subsystems.ir_blaster import send_IR

def test_all_antennae():
    """
    Sends a test transmission to all 4 antennae with different colors.
    Ant 1 -> Red
    Ant 2 -> Green
    Ant 3 -> Blue
    Ant 4 -> Purple
    """
    test_sequence = [
        (1, 'R'),
        (2, 'G'),
        (3, 'B'),
        (4, 'P')
    ]
    
    print("==================================================")
    print(" Starting IR Blaster Test: All Antennae & Colors")
    print("==================================================")
    
    for ant, color in test_sequence:
        print(f"\n[TEST] Sending -> Antenna {ant}, Color '{color}'")
        success = send_IR(ant, color)
        if success:
            print("  -> ir-ctl command returned success.")
        else:
            print("  -> ERROR: ir-ctl command failed.")
            
        time.sleep(2) # Give the receiver time to process and update LCD
        
    print("\n[OK] Test sequence complete.")

if __name__ == "__main__":
    test_all_antennae()
