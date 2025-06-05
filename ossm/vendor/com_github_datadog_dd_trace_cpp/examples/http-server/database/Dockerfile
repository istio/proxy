from alpine:3.17

run apk update && \
    apk add python3 py3-pip && \
    pip install ddtrace flask gevent

copy database.py /usr/local/bin/database.py

cmd ["ddtrace-run", "python", "/usr/local/bin/database.py"]
