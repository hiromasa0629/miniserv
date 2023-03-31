import subprocess
import time

HOST = "localhost"
PORT = 8080

for i in range(100):
    cmd = ["nc", "-z", HOST, str(PORT)]
    proc = subprocess.Popen(cmd)
