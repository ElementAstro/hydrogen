#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Custom Filter Wheel Example

This example shows how to create a custom filter wheel by inheriting from the FilterWheel base class.
It demonstrates how to override virtual methods to implement custom behavior.
"""

import time
import threading
from pyastrodevice import PyFilterWheel

class CustomFilterWheel(PyFilterWheel):
    """Custom filter wheel implementation with faster movement and custom filters"""
    
    def __init__(self, device_id, manufacturer="CustomWheel", model="FastWheel"):
        """Initialize custom filter wheel with specific parameters"""
        super().__init__(device_id, manufacturer, model)
        
        # Override the default filter configuration
        self._movement_speed = 0.5  # positions per second (2x faster than base class)
        self._custom_thread = None
        self._is_running = False
        
    def start(self):
        """Override start method to add custom initialization"""
        print(f"Starting custom filter wheel: {self._device_id}")
        
        # Set custom filter names and offsets
        self.set_filter_count(8)  # Using 8 slots instead of default 5
        self.set_filter_names(["Red", "Green", "Blue", "OIII", "SII", "Ha", "Luminance", "IR-Cut"])
        self.set_filter_offsets([0, 0, 0, 100, 120, 150, 0, 0])  # Focus offsets in steps
        
        # Start custom monitoring thread
        self._is_running = True
        self._custom_thread = threading.Thread(target=self._monitor_temperature)
        self._custom_thread.daemon = True
        self._custom_thread.start()
        
        # Start the base device
        return super().start()
    
    def stop(self):
        """Override stop method to handle custom resources"""
        print(f"Stopping custom filter wheel: {self._device_id}")
        self._is_running = False
        
        # Stop the custom thread
        if self._custom_thread and self._custom_thread.is_alive():
            self._custom_thread.join(timeout=2.0)
        
        # Stop the base device
        super().stop()
    
    def get_max_filter_count(self):
        """Override to support more filters"""
        return 12  # This custom wheel can handle up to 12 filters
    
    def _monitor_temperature(self):
        """Custom monitoring function that runs in a separate thread"""
        while self._is_running:
            # Simulate temperature monitoring
            temperature = 20.0 + (0.1 * (time.time() % 10))  # Simulate temperature fluctuations
            self.set_property("temperature", round(temperature, 1))
            time.sleep(2.0)

    def simulate_movement(self, elapsed_sec):
        """Override movement simulation for faster movement speed"""
        # This method would normally be called internally by the C++ base class
        # In Python, we demonstrate it for educational purposes only
        # In a real implementation, you'd need additional code to make this work properly
        print(f"Custom simulation - elapsed: {elapsed_sec:.2f}s")


def main():
    """Example of using the custom filter wheel"""
    # Create a custom filter wheel
    fw = CustomFilterWheel("CUSTOM-FW1")
    
    try:
        # Start the filter wheel
        if fw.start():
            print(f"Filter wheel started successfully")
            print(f"Max filter count: {fw.get_max_filter_count()}")
            
            # Set position by number
            print("Moving to position 3...")
            fw.set_position(3)
            
            # Wait for movement to complete
            while not fw.is_movement_complete():
                time.sleep(0.1)
                
            print(f"Current filter: {fw.current_filter}")
            print(f"Current offset: {fw.current_offset}")
            
            # Test the abort function
            print("Moving to position 7...")
            fw.set_position(7)
            time.sleep(0.5)  # Let it move a bit
            print("Aborting movement...")
            fw.abort()
            
            # Demonstrate property access
            time.sleep(3.0)  # Let the temperature monitoring run
            temperature = fw.get_property("temperature")
            print(f"Current filter wheel temperature: {temperature}°C")
            
        else:
            print("Failed to start filter wheel")
            
    finally:
        # Always stop the device
        fw.stop()
        print("Filter wheel stopped")


if __name__ == "__main__":
    main()