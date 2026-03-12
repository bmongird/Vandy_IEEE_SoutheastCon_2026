import multiprocessing
import logging
import time

try:
    from subsystems.lcd_display import AntennaDisplay
    LCD_AVAILABLE = True
except ImportError:
    LCD_AVAILABLE = False
    logging.warning("lcd_test module not found. LCDSubsystem will mock operations.")

class LCDSubsystem:
    """
    Subsystem for the LCD Display.
    Manages a separate process for the LCD display to run continuously.
    """
    def __init__(self):
        self.logger = logging.getLogger("LCDSubsystem")
        self.manager = multiprocessing.Manager()
        self.cmd_queue = self.manager.Queue()
        
        self.process = multiprocessing.Process(
            target=self._run_lcd_loop,
            args=(self.cmd_queue,),
            daemon=True
        )
        self.process.start()
        self.logger.info("LCD subsystem initialized and running on a separate process.")

    @staticmethod
    def _run_lcd_loop(cmd_queue):
        """
        Static method to run the LCD loop in a separate process.
        """
        if LCD_AVAILABLE:
            display = AntennaDisplay()
            display.start()
            print("LCD Process Running with AntennaDisplay...")
        else:
            display = None
            print("LCD Process Running in Mock mode...")

        try:
            while True:
                # Process all pending commands
                while not cmd_queue.empty():
                    cmd = cmd_queue.get_nowait()
                    if cmd['type'] == 'set_antenna':
                        antenna = cmd['antenna']
                        color = cmd['color']
                        if display:
                            try:
                                display.set_antenna(antenna, color)
                            except Exception as e:
                                print(f"Error setting antenna: {e}")
                        else:
                            print(f"[Mock LCD] Set Antenna {antenna} to {color}")
                
                # Sleep to prevent busy waiting
                time.sleep(0.05)
        except KeyboardInterrupt:
            pass
        finally:
            if display:
                display.stop()

    def set_antenna(self, antenna: int, color: str):
        """
        Schedules setting the antenna color on the LCD process.
        """
        self.cmd_queue.put({'type': 'set_antenna', 'antenna': antenna, 'color': color})

    def stop(self):
        """
        Stops the LCD process.
        """
        if self.process.is_alive():
            self.process.terminate()
            self.process.join()
