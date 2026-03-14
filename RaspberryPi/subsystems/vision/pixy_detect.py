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
ROI_Y_MAX = 40

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

# module‑level initialization state ------------------------------------------------
_pixy_initialized = False
_pixy_init_result = None


def _ensure_pixy_initialized() -> int:
    """Make sure the Pixy2 library is initialized exactly once.

    Returns:
        The value returned by ``pixy.init()`` (0 on success, negative on
        failure).  Subsequent calls return the original result without
        re‑initializing the camera.
    """
    global _pixy_initialized, _pixy_init_result
    if _pixy_initialized:
        return _pixy_init_result

    _pixy_init_result = pixy.init()
    _pixy_initialized = True
    if _pixy_init_result < 0:
        logging.error(f"Pixy2 initialization failed with code {_pixy_init_result}")
    else:
        # set CCC program once
        try:
            pixy.change_prog("color_connected_components")
        except Exception as exc:  # pragma: no cover - pixy is C extension
            logging.warning(f"couldn't set CCC program during init: {exc}")
    return _pixy_init_result


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
    
    # initialize once; subsequent invocations will be no-ops
    init_res = _ensure_pixy_initialized()
    if init_res < 0:
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

import cv2
import numpy as np
import os

def classify_led_color(image_path, debug=False):
    # OpenCV loads images in BGR format
    img = cv2.imread(image_path)
    if img is None:
        return "Error: Image not found"
        
    # --- ROI EXTRACTION: Tightened to exclude background glare ---
    height, width = img.shape[:2]
    
    start_y = 0
    end_y = height // 10 
    start_x = width // 3
    end_x = 2 * (width // 3)
    
    roi_img = img[start_y:end_y, start_x:end_x]
    roi_hsv = cv2.cvtColor(roi_img, cv2.COLOR_BGR2HSV)

    # --- SPECIAL CASE: EXPLICIT RED PIXEL HUNT ---
    # Because the red LED is dim, it might not be the brightest object.
    # We explicitly search for red hues with low Value/Saturation thresholds.
    lower_red1 = np.array([0, 40, 20])   
    upper_red1 = np.array([15, 255, 255])
    lower_red2 = np.array([160, 40, 20])
    upper_red2 = np.array([179, 255, 255])

    mask_red1 = cv2.inRange(roi_hsv, lower_red1, upper_red1)
    mask_red2 = cv2.inRange(roi_hsv, lower_red2, upper_red2)
    mask_red_total = cv2.bitwise_or(mask_red1, mask_red2)

    red_pixel_count = cv2.countNonZero(mask_red_total)

    if debug:
        os.makedirs("debug_output", exist_ok=True)
        cv2.imwrite("debug_output/00_roi_img.png", roi_img)
        # Save this to see exactly which pixels triggered the Red bypass
        cv2.imwrite("debug_output/00a_red_mask.png", mask_red_total) 

    # If we find a small cluster of red pixels, exit early with 'R'
    print(red_pixel_count)
    if red_pixel_count >= 500:
        return "R"
    # ---------------------------------------------

    # 1. Isolate the Light Source (Fallback for Green, Blue, Purple)
    v_channel = roi_hsv[:, :, 2]
    
    _, max_val, _, _ = cv2.minMaxLoc(v_channel)
    dynamic_threshold = max(max_val - 10, 50) 
    
    _, core_mask = cv2.threshold(v_channel, dynamic_threshold, 255, cv2.THRESH_BINARY)
    
    kernel = np.ones((5, 5), np.uint8)
    dilated_core = cv2.dilate(core_mask, kernel, iterations=1)
    fringe_mask = cv2.bitwise_xor(dilated_core, core_mask)

    # --- DEBUG SAVING BLOCK ---
    if debug:
        cv2.imwrite("debug_output/01_v_channel.png", v_channel)
        cv2.imwrite("debug_output/02_core_mask.png", core_mask)
        cv2.imwrite("debug_output/03_fringe_mask.png", fringe_mask)
        
        core_pixels = cv2.bitwise_and(roi_img, roi_img, mask=core_mask)
        fringe_pixels = cv2.bitwise_and(roi_img, roi_img, mask=fringe_mask)
        
        cv2.imwrite("debug_output/04_core_pixels.png", core_pixels)
        cv2.imwrite("debug_output/05_fringe_pixels.png", fringe_pixels)
        print("Debug images saved to /debug_output/")
    # --------------------------

    # 2. Extract Data from Core and Fringe
    core_hsv_mean = cv2.mean(roi_hsv, mask=core_mask)
    fringe_bgr_mean = cv2.mean(roi_img, mask=fringe_mask)

    core_h = core_hsv_mean[0]  
    core_s = core_hsv_mean[1]  
    core_v = core_hsv_mean[2]  

    fringe_b = fringe_bgr_mean[0]
    fringe_g = fringe_bgr_mean[1]
    fringe_r = fringe_bgr_mean[2]

    # If the mask is entirely dark (target wasn't found), exit early
    if core_v < 10: 
        return "None"

    # 3. Classification Logic
    print(core_h)
        
    # Check for Green ('G')
    if 80 < core_h < 88:
        return "G"

    # Differentiate Blue ('B') vs. Purple ('P')
    else:
        print(core_s)
        print(fringe_r)
        print(fringe_g)
        if core_s < 120 and core_s > 110:
            return "P"
        else:
            return "B"

# Example usage:
result = classify_led_color("image10.png", debug=True)
print(f"Detected Target: {result}")

if __name__ == "__main__":
    # Simple test routine when run as a script
    logging.basicConfig(level=logging.DEBUG)
    print("Testing Pixy2 LED Detection (Timeout: 5s)...")
    result = detect_led_color(5.0)
    print(f"Detection Result: {result}")
