import requests
import serial
import time
import os

# Serial port for ESP32 this is hte best program it is able to make esp32 to send signal back to frdm k64f
esp_serial = serial.Serial('COM13', 115200, timeout=5)  # ESP32 COM port

# Directory to save captured images
output_dir = "captured_images"
os.makedirs(output_dir, exist_ok=True)

def process_api_response(api_response):
    """Process the API response and extract health status and confidence."""
    if not isinstance(api_response, dict):
        print("Error: API response is not a dictionary:", api_response)
        return {"status": "unknown", "confidence": 0.0}

    status = api_response.get("status", "unknown")
    confidence = api_response.get("confidence", 0.0)
    return {"status": status, "confidence": confidence}

def receive_image():
    """Receive image data from ESP32."""
    print("Waiting for image data...")
    start_marker = b"START_IMAGE"
    end_marker = b"END_IMAGE"

    # Flush input buffer to clear any old data
    esp_serial.reset_input_buffer()

    # Wait for start marker with timeout
    start_time = time.time()
    while (time.time() - start_time) < 30:  # 30-second timeout
        line = esp_serial.readline().strip()
        print(f"Read: {line}")  # Debug output
        if start_marker in line:
            print("Start marker received!")
            break
    else:
        print("Timeout waiting for start marker")
        return None

    # Get image size
    size_line = esp_serial.readline().strip()
    try:
        size = int(size_line.split(b":")[1])
        print(f"Image size: {size} bytes")
    except (IndexError, ValueError) as e:
        print(f"Error parsing image size: {e}, received: {size_line}")
        return None

    # Read image data with timeout
    image_data = bytearray()
    bytes_read = 0
    timeout = time.time() + 30  # 30-second timeout

    while bytes_read < size and time.time() < timeout:
        chunk = esp_serial.read(min(1024, size - bytes_read))
        if chunk:
            image_data.extend(chunk)
            bytes_read += len(chunk)
            print(f"Read {bytes_read}/{size} bytes")

    if bytes_read < size:
        print(f"Warning: Only read {bytes_read}/{size} bytes")

    # Verify end marker
    end_line = esp_serial.readline().strip()
    if end_marker in end_line:
        print("End marker received!")
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        filename = os.path.join(output_dir, f"image_{timestamp}.jpg")
        with open(filename, "wb") as f:
            f.write(image_data)
        print(f"Image saved as {filename}")
        return filename
    else:
        print(f"Error: End marker not received. Got: {end_line}")
        return None

def send_image_to_api(image_path):
    """Send image to API and process response."""
    try:
        with open(image_path, 'rb') as image_file:
            response = requests.post(
                'http://localhost:8000/detect',  # Replace with your API endpoint
                files={'file': image_file},
                timeout=10  # Add timeout to avoid hanging requests
            )
        print("Raw API Response:", response.text)
        api_response = response.json()
        return process_api_response(api_response)
    except Exception as e:
        print(f"Error sending image to API: {str(e)}")
        return {"status": "unknown", "confidence": 0.0}

def communicate_with_esp32(status):
    """Send health status to ESP32."""
    try:
        if status == "unhealthy":
            print("Sending unhealthy signal to ESP32...")
            esp_serial.write(b'D\n')  # Changed to match what ESP32 expects
            esp_serial.flush()  # Ensure data is sent immediately

            # Wait for acknowledgment from ESP32
            time.sleep(0.5)
            while esp_serial.in_waiting:
                response = esp_serial.readline().decode('utf-8', errors='ignore').strip()
                print(f"ESP32 response: {response}")
                if "SPRAY_SIGNAL_SENT" in response:
                    print("Spray system activated via ESP32.")
                    break

        elif status == "healthy":
            print("Sending healthy signal to ESP32...")
            esp_serial.write(b'H\n')  # Send healthy signal
            esp_serial.flush()  # Ensure data is sent immediately

            # Wait for acknowledgment from ESP32
            time.sleep(0.5)
            while esp_serial.in_waiting:
                response = esp_serial.readline().decode('utf-8', errors='ignore').strip()
                print(f"ESP32 response: {response}")

    except Exception as e:
        print(f"Error communicating with ESP32: {str(e)}")

def main():
    """Main function to coordinate image capture and processing."""
    print("Smart Health Monitor and Spray System Starting...")

    while True:
        try:
            # Receive image from ESP32
            image_path = receive_image()
            if image_path:
                # Send image to API and get health status
                result = send_image_to_api(image_path)
                health_status = result.get("status", "unknown")
                confidence = result.get("confidence", 0.0)
                print(f"Health Status: {health_status}, Confidence: {confidence}")

                # Communicate with ESP32 based on health status
                communicate_with_esp32(health_status)
            else:
                print("Failed to receive valid image, retrying...")

            time.sleep(5)  # Delay between iterations
        except KeyboardInterrupt:
            print("Program terminated by user.")
            break
        except Exception as e:
            print(f"Unexpected error: {str(e)}")
            time.sleep(5)  # Wait before retrying after an error

if __name__ == "__main__":
    main()