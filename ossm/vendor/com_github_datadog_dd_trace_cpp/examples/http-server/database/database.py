"""database - a thin wrapper around a sqlite3 database

This is an HTTP server that exposes a SQLite 3 database via the following
endpoints:

    GET /query?sql=<sql>
        Execute the <sql> against the database in read-only mode. Return
        application/json containing an array of the resulting rows, where
        each row is an array.

        If an error occurs, return a text/plain description of the error.

    GET /execute?sql=<sql>
        Execute the <sql> against the database in read-write mode.
        On success, return status 200 with an empty body.

        If an error occurs, return a text/plain description of the error.

The database initially contains the following table:

    create table Note(
        AddedWhen text,
        Body text);
"""

import flask
import gevent
from gevent.pywsgi import WSGIServer
import signal
import sqlite3
import sys

DB_FILE_PATH = '/tmp/database.sqlite'

app = flask.Flask(__name__)


@app.route('/')
def hello():
    return "Hello, World!"


@app.route('/query')
def query():
    request = flask.request
    sql = request.args.get('sql')
    if sql is None:
        return ('"sql" query parameter is required.', 400)

    with sqlite3.connect(f'file:{DB_FILE_PATH}?mode=ro') as db:
        try:
            return list(db.execute(sql))
        except Exception as error:
            return str(error) + '\n', 400


@app.route('/execute')
def execute():
    request = flask.request
    sql = request.args.get('sql')
    if sql is None:
        return ('"sql" query parameter is required.', 400)

    with sqlite3.connect(DB_FILE_PATH) as db:
        try:
            db.execute(sql)
            return ''
        except Exception as error:
            return str(error) + '\n', 400


if __name__ == '__main__':
    gevent.signal_handler(signal.SIGTERM, sys.exit)

    with sqlite3.connect(DB_FILE_PATH) as db:
        db.execute('''
        create table if not exists Note(
            AddedWhen text,
            Body text);
        ''')

    http_server = WSGIServer(('', 80), app)
    http_server.serve_forever()
