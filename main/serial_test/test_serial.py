import time
from flask import Flask, Response
app = Flask(__name__)

@app.route('/stream')
def stream():
    def generate():
        while True:
            try:
                with open('image.jpg', 'rb') as f:
                    yield (b'--frame\r\n'
                           b'Content-Type: image/jpeg\r\n\r\n' + f.read() + b'\r\n')
                time.sleep(0.1)
            except:
                time.sleep(0.1)
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')

app.run(host='0.0.0.0', port=5000)