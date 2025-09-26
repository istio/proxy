import requests

from skywalking import agent, config
from skywalking.decorators import runnable

if __name__ == '__main__':
    config.init()
    agent.start()

    from flask import Flask, Response

    app = Flask(__name__)

    @app.route("/users", methods=["POST", "GET"])
    def application():
        res = requests.get("http://provider:8081/pong2")
        return Response(status=res.status_code)

    PORT = 8082
    app.run(host='0.0.0.0', port=PORT, debug=True)
