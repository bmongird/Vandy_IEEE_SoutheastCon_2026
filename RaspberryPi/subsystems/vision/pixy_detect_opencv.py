import time
import logging
import cv2
import numpy as np
import os
import ctypes

try:
    import pixy
    from pixy import *
    PIXY_AVAILABLE = True
except ImportError:
    import builtins
    if not hasattr(builtins, "PIXY_AVAILABLE"):
        PIXY_AVAILABLE = False
        logging.warning("Pixy module not found. detect_led_color will return 'F' or run in mock mode.")


# module-level initialization state ------------------------------------------------
_pixy_initialized = False
_pixy_init_result = None


def _ensure_pixy_initialized() -> int:
    """Make sure the Pixy2 library is initialized exactly once.

    Returns:
        The value returned by ``pixy.init()`` (0 on success, negative on
        failure). Subsequent calls return the original result without
        re-initializing the camera.
    """
    global _pixy_initialized, _pixy_init_result
    if _pixy_initialized:
        return _pixy_init_result

    if not PIXY_AVAILABLE:
        return -1

    _pixy_init_result = pixy.init()
    _pixy_initialized = True
    if _pixy_init_result < 0:
        logging.error(f"Pixy2 initialization failed with code {_pixy_init_result}")
    else:
        # set video program once
        try:
            pixy.change_prog("video")
        except Exception as exc:  # pragma: no cover
            logging.warning(f"couldn't set video program during init: {exc}")
    return _pixy_init_result


def grab_roi_from_pixy(width, height) -> np.ndarray:
    """
    Grabs only the Region of Interest from the Pixy2 using video_get_RGB.
    Instead of getting the full 316x208 frame, we just fetch the pixels we need
    to speed up Python operations.
    
    The OpenCV code expects an ROI extracted as:
    start_y = 0
    end_y = height // 10 
    start_x = width // 3
    end_x = 2 * (width // 3)
    """
    start_y = 0
    end_y = height // 10 
    start_x = width // 3
    end_x = 2 * (width // 3)
    
    roi_width = end_x - start_x
    roi_height = end_y - start_y
    
    # Pre-allocate ROI array in BGR format (for OpenCV compatibility)
    roi_bgr = np.zeros((roi_height, roi_width, 3), dtype=np.uint8)

    # RGB ctypes pointer array wrapper that Pixy API expects
    pixy.video_get_RGB.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]
    rgb_arr = (ctypes.c_uint8 * 3)()

    # Iterate over the ROI pixels only
    for y in range(start_y, end_y):
        for x in range(start_x, end_x):
            try:
                # API Call takes X, Y, pointer to dest, and bool sRGB (0 for raw, 1 for sRGB)
                # In python SWIG video_get_RGB actually returns int directly if you use it like:
                # r, g, b = pixy.video_get_RGB(x, y) if wrapper is made that way.
                # Looking at original SWIG bindings, video_get_RGB(x,y) returns a tuple or int. Let's do a safe try loop
                # First attempt the simple return tuple implementation
                ret = pixy.video_get_RGB(x, y) 
                
                # Handling if video_get_RGB returns a 3 tuple or a struct
                # Pixy returns: 0 for error, or an RGB struct depending on SWIG config. 
                # Let's assume the default `def video_get_RGB(X, Y):` wrapper returns an int (RGB888) or tuple.
                if isinstance(ret, tuple) and len(ret) >= 3:
                     r, g, b = ret[:3]
                else:
                    # In many Pixy2 default python interfaces video_get_RGB(X,Y) packs RGB as an integer OR returns nothing because of missing pointer.
                    # We will use the simplest assumed packing if int:
                    # R = (ret >> 16) & 0xFF, G = (ret >> 8) & 0xFF, B = ret & 0xFF
                    # Let's add logging if this fails
                    val = int(ret)
                    r = (val >> 16) & 0xFF
                    g = (val >> 8) & 0xFF
                    b = val & 0xFF

                # Store in BGR format
                # Note: indices for roi_bgr are y-start_y and x-start_x
                roi_bgr[y - start_y, x - start_x] = [b, g, r]

            except Exception as e:
                logging.error(f"Failed to grab RGB from Pixy at ({x},{y}): {e}")
                return None
                
    return roi_bgr


def classify_led_color_from_frame(roi_img: np.ndarray, debug: bool = False) -> str:
    """
    OpenCV based classification. Expects `roi_img` (BGR format) that was already cropped
    to the specific ROI dimensions.
    """
    if roi_img is None or roi_img.size == 0:
        return 'F'
        
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
        cv2.imwrite("debug_output/00a_red_mask.png", mask_red_total) 

    # If we find a small cluster of red pixels, exit early with 'R'
    if debug:
        print(f"Red pixel count: {red_pixel_count}")
    
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
        return "F"

    # 3. Classification Logic
    if debug:
        print(f"Core Hue: {core_h}, Core Saturation: {core_s}")
        print(f"Fringe RGB: ({fringe_r}, {fringe_g}, {fringe_b})")
        
    # Check for Green ('G')
    if 80 < core_h < 88:
        return "G"

    # Differentiate Blue ('B') vs. Purple ('P')
    else:
        if 110 < core_s < 120:
            return "P"
        else:
            return "B"


def detect_led_color(timeout_seconds: float = 5.0, debug: bool = False) -> str:
    """
    Attempts to read the LED color from the Pixy2 camera within the specified timeout
    by fetching real Pixels from the ROI and feeding it to OpenCV.
    
    Args:
        timeout_seconds: Maximum time to wait for a valid color detection.
        debug: Saves intermediate mask arrays if set to true.
        
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

    try:
        # Standard Pixy2 Resolution defaults
        # But we could also call pixy.get_frame_width() and pixy.get_frame_height()
        width = pixy.get_frame_width()
        height = pixy.get_frame_height()
        if width == 0 or height == 0:
             width, height = 316, 208
    except:
        width, height = 316, 208

    # Poll until timeout
    while (time.time() - start_time) < timeout_seconds:
        # Switch to video mode. Needs to happen in case camera reset from another program.
        pixy.change_prog("video")
        
        # Capture ROI
        roi_img = grab_roi_from_pixy(width, height)
        
        if roi_img is not None:
            # Pass image to openCV logic
            color_char = classify_led_color_from_frame(roi_img, debug=debug)
            if color_char in ('R', 'G', 'B', 'P'):
                return color_char
                
        # Small sleep to prevent tightlooping 100% CPU on the Pi
        time.sleep(0.01)
        
    logging.warning(f"Pixy2 detection timed out after {timeout_seconds}s. Returning 'F'.")
    return 'F'


if __name__ == "__main__":
    # Simple test routine when run as a script
    logging.basicConfig(level=logging.DEBUG)
    print("Testing Pixy2 LED Detection (Timeout: 5s)...")
    result = detect_led_color(timeout_seconds=5.0, debug=True)
    print(f"Detection Result: {result}")
