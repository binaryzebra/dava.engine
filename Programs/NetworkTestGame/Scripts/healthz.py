from __future__ import print_function
import sys
import enet
import struct
import getopt
import time
import httplib
from flask import Flask
from apscheduler.schedulers.background import BackgroundScheduler
import logging
import glob
import os

logging.basicConfig(level=logging.INFO)
logging.getLogger("apscheduler.executors.default").setLevel(logging.ERROR)
logger = logging.getLogger(__name__)
server_status = httplib.SERVICE_UNAVAILABLE
last_ts = 0
cron = BackgroundScheduler(daemon=True)
cron.start()
app = Flask(__name__)


def upload_crashdumps():
    # TODO: attach attributes: version, etc - where to get them?
    token='c45627c0e8e7877a9e72d8c62a333e3a3b10c7545a28038f840fb6d2707c319f'
    # Use trial Backtrace.io account
    version=os.environ.get('VERSION', 'nover')
    # Do not upload crashdump if version has not been specified
    if version != 'nover':
        url='https://kkdaemon.sp.backtrace.io:6098/post?format={}&token={}&version={}'.format('minidump', token, version)
        all_crashdumps=glob.glob('/debug/server/*.dmp')
        for crashdump in all_crashdumps:
            cmd = 'curl --data-binary @{} "{}"'.format(crashdump, url)
            try:
                os.system(cmd)
                os.remove(crashdump)
            except:
                pass


def handle_event(event):
    global server_status
    global last_ts
    timestamp = int(time.time())
    if event.type == enet.EVENT_TYPE_CONNECT:
        logger.info('%s: Service connected' % timestamp)
        server_status = httplib.OK
    elif event.type == enet.EVENT_TYPE_DISCONNECT:
        logger.info('%s: Service disconnected' % timestamp)
        server_status = httplib.INTERNAL_SERVER_ERROR
    elif event.type == enet.EVENT_TYPE_RECEIVE:
        server_status = int(struct.unpack('H', event.packet.data[1:])[0])
        last_ts = timestamp
    if server_status == httplib.OK and last_ts != 0 and timestamp - last_ts > 1:
        logger.info('%s: Service is unavailable' % timestamp)
        server_status = httplib.SERVICE_UNAVAILABLE
    if server_status == httplib.SERVICE_UNAVAILABLE:
        upload_crashdumps()

def enet_service(host):
    beginSec = time.time()
    while (time.time() - beginSec < 0.5):
        event = host.service(0)
        handle_event(event)

@app.route('/healthz', methods=['GET'])
def healthz():
    logger.info('%s: /healthz %s' % (int(time.time()), server_status))
    return '', server_status

if __name__ == '__main__':
    optlist, _ = getopt.getopt(sys.argv[1:], '', ['enet-port=','http-port='])
    http_port = 8080
    enet_port = 5050
    for opt in optlist:
        if opt[0] == '--http-port':
            http_port = int(opt[1])
        elif opt[0] == '--enet-port':
            enet_port = int(opt[1])

    host = enet.Host(enet.Address('0.0.0.0', enet_port), 1, 0, 0)
    cron.add_job(enet_service, 'interval', seconds=1, args=[host])
    app.run(host='0.0.0.0', port=http_port)
