import os
import time
import requests

DJANGO_UPLOAD_URL = "http://192.168.1.128:8000/upload_heart_data/"
FILES_DIR = "."  # Ścieżka do folderu, w którym są HR1.txt itd.

def upload_file(file_name):
    file_path = os.path.join(FILES_DIR, file_name)
    if os.path.exists(file_path):
        try:
            with open(file_path, 'rb') as f:
                files = {'file': (file_name, f)}
                response = requests.post(DJANGO_UPLOAD_URL, files=files)
                print(f"[OK] {file_name} -> {response.status_code}")
        except Exception as e:
            print(f"[ERR] {file_name}: {e}")
    else:
        print(f"[INFO] Plik {file_name} nie istnieje.")

def main_loop():
    while True:
        for i in range(1, 5):
            file_name = f"HR{i}.txt"
            upload_file(file_name)
        time.sleep(1)

if __name__ == "__main__":
    main_loop()
