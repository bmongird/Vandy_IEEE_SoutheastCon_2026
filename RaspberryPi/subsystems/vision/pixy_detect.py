import time
import logging

try:
    import pixy
    from ctypes import *
    from pixy import *
    PIXY_AVAILABLE = True
except ImportError:
    PIXY_AVAILABLE = False
    logging.warning("Pixy module not found. detect_led_color will return 'F' or run in mock mode.")

# ---- ROI Configuration ----
# You may need to adjust these values based on your physical setup.
# Run a calibration script or PixyMon to find the exact frame region the LED appears in.
ROI_X_MIN = 130
ROI_X_MAX = 190
ROI_Y_MIN = 10
ROI_Y_MAX = 30

# ---- Signature-to-Color Mapping ----
# 1=green, 2=red, 3=blue, 4=purple
SIG_COLORS = {
    1: 'G',
    2: 'R',
    3: 'B',
    4: 'P'
}

def get_blocks_in_roi(blocks, count):
    """Filter detected blocks to only those within the ROI."""
    roi_blocks = []
    for i in range(count):
        b = blocks[i]
        if (ROI_X_MIN <= b.m_x <= ROI_X_MAX and
                ROI_Y_MIN <= b.m_y <= ROI_Y_MAX):
            roi_blocks.append(b)
    return roi_blocks

def get_best_block(roi_blocks):
    """Return the largest block in the ROI, or None if empty."""
    if not roi_blocks:
        return None
    return max(roi_blocks, key=lambda b: b.m_width * b.m_height)

def detect_led_color(timeout_seconds: float = 5.0) -> str:
    """
    Attempts to read the LED color from the Pixy2 camera within the specified timeout.
    
    Args:
        timeout_seconds: Maximum time to wait for a valid color detection.
        
    Returns:
        One of 'R', 'G', 'B', 'P' if a color is detected successfully.
        'F' if no connection, timeout, or otherwise fail to detect any valid color.
    """
    if not PIXY_AVAILABLE:
        logging.error("Cannot use detect_led_color: pixy library unavailable. Returning 'F'.")
        return 'F'
        
    start_time = time.time()
    
    # Initialize Pixy2
    # pixy.init() returns 0 on success, < 0 on error
    init_res = pixy.init()
    if init_res < 0:
        logging.error(f"Pixy2 initialization failed with code {init_res}. Returning 'F'.")
        return 'F'
        
    # Change program to Color Connected Components (CCC) mode
    pixy.change_prog("color_connected_components")
    
    # Allocate a BlockArray for receiving data from the camera
    blocks = BlockArray(100)
    
    # Poll until timeout
    while (time.time() - start_time) < timeout_seconds:
        # Request up to 100 blocks
        count = pixy.ccc_get_blocks(100, blocks)
        
        if count > 0:
            roi_blocks = get_blocks_in_roi(blocks, count)
            best_block = get_best_block(roi_blocks)
            
            if best_block is not None:
                color_char = SIG_COLORS.get(best_block.m_signature, None)
                if color_char:
                    return color_char
                else:
                    logging.debug(f"Detected signature {best_block.m_signature} in ROI, but not mapped to a known color.")
                    
        # Small sleep to prevent tightlooping 100% CPU on the Pi
        time.sleep(0.01)
        
    logging.warning(f"Pixy2 detection timed out after {timeout_seconds}s. Returning 'F'.")
    return 'F'

if __name__ == "__main__":
    # Simple test routine when run as a script
    logging.basicConfig(level=logging.DEBUG)
    print("Testing Pixy2 LED Detection (Timeout: 5s)...")
    result = detect_led_color(5.0)
    print(f"Detection Result: {result}")
