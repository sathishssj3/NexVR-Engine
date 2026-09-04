import os
import subprocess
import sys

if __name__ == '__main__':
    client_script = os.path.join(os.path.dirname(__file__), 'nexvr-client', 'generate_dummy_onnx.py')
    if os.path.exists(client_script):
        res = subprocess.run([sys.executable, client_script], cwd=os.path.dirname(client_script))
        sys.exit(res.returncode)
    else:
        sys.exit(1)
