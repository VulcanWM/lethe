import os
import sys
import time
import serial
import serial.tools.list_ports

BAUD_RATE = 115200

def find_port():
    ports = list(serial.tools.list_ports.comports())

    if not ports:
        print("no serial ports found")
        sys.exit(1)

    print("available ports: ")

    for i, port in enumerate(ports):
        print(f"{i}: {port.device} - {port.description}")

    choice = int(input("pick lethe's port: "))
    return ports[choice].device

def upload_file(port_name, file_path, set_type):
    if set_type not in ["MCQ", "FLASHCARD"]:
        print("type must be MCQ or FLASHCARD")
        return

    if not os.path.exists(file_path):
        print("file does not exist")
        return

    filename = os.path.basename(file_path)

    with open(file_path, "rb") as f:
        data = f.read()

    file_size = len(data)

    with serial.Serial(port_name, BAUD_RATE, timeout=5) as ser:
        time.sleep(2)

        ser.write(b"UPLOAD\n")
        ser.write((set_type + "\n").encode())
        ser.write((filename + "\n").encode())
        ser.write((str(file_size) + "\n").encode())

        ser.write(data)
        ser.flush()

        print(f"sent {file_size} bytes")

        response = ser.readline().decode(errors="ignore").strip()

        if response == "OK":
            print("upload successful")
        else:
            print("device response: ", response)

def main():
    if len(sys.argv) != 3:
        print("usage:")
        print("python upload.py <MCQ|FLASHCARD> <file.json>")
        return

    set_type = sys.argv[1].upper()
    file_path = sys.argv[2]

    port = find_port()

    upload_file(port, file_path, set_type)

if __name__ == "__main__":
    main()