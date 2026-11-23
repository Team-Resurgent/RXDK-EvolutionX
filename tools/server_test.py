# Talk to EvoX's FTP server in xemu (NAT, forwarded to host ports) and print a transcript, so the
# original's and the rebuild's answers can be diffed. Only safe commands: no FLASHBIOS, EXECUTE,
# FORMATPATH or FORMATDRIVE, and no LIST of "/" (it validates F:, which can format it); files
# are written only under E:\evoxtest, which is removed again.
# The server has no PASV: data connections use PORT to the host as the guest sees it (10.0.2.2).
# usage: python tools/server_test.py ftp [host port]
import socket, sys, threading, time

HOST = '127.0.0.1'
GUEST_HOST = (10, 0, 2, 2)  # the host, from inside xemu's NAT
DATA_PORT = 2120


def recv_reply(s, quiet=1.0):
    # everything the server sends until it has been quiet for a while: the server takes only the
    # first line of each receive, so a command must not be sent while replies are still coming
    data = b''
    s.settimeout(10)
    while True:
        try:
            chunk = s.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
        s.settimeout(quiet)
    return data.decode('latin-1')


class DataListener:
    def __init__(self):
        self.srv = socket.socket()
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(('127.0.0.1', DATA_PORT))
        self.srv.listen(1)
        self.srv.settimeout(15)
        self.received = None
        self.send = None

    def port_command(self):
        return 'PORT %d,%d,%d,%d,%d,%d' % (GUEST_HOST + (DATA_PORT >> 8, DATA_PORT & 0xFF))

    def run(self):
        try:
            c, _ = self.srv.accept()
        except socket.timeout:
            self.received = b'(no data connection)'
            return
        if self.send is not None:
            c.sendall(self.send)
            c.close()
            self.received = b''
            return
        buf = b''
        c.settimeout(10)
        while True:
            try:
                d = c.recv(65536)
            except socket.timeout:
                break
            if not d:
                break
            buf += d
        c.close()
        self.received = buf

    def close(self):
        self.srv.close()


def ftp(port):
    s = socket.create_connection((HOST, port), timeout=10)
    out = []

    def show(text):
        out.append(text)
        print(text, end='' if text.endswith('\n') else '\n')

    show('<< ' + recv_reply(s))

    def cmd(line):
        show('>> ' + line)
        s.sendall((line + '\r\n').encode('latin-1'))
        show('<< ' + recv_reply(s))

    def data_cmd(line, payload=None):
        dl = DataListener()
        dl.send = payload
        t = threading.Thread(target=dl.run)
        t.start()
        cmd(dl.port_command())
        show('>> ' + line)
        s.sendall((line + '\r\n').encode('latin-1'))
        t.join(20)
        dl.close()
        reply = recv_reply(s, 2.0)
        if payload is None:
            show('[data %d bytes]\n%s' % (len(dl.received), dl.received.decode('latin-1')))
        show('<< ' + reply)

    cmd('USER xbox')
    cmd('PASS wrong')
    s.close()
    s = socket.create_connection((HOST, port), timeout=10)
    show('<< ' + recv_reply(s))
    cmd('USER xbox')
    cmd('PASS xbox')
    cmd('SYST')
    cmd('PWD')
    cmd('TYPE I')
    cmd('TYPE A')
    cmd('NOOP')
    # (never LIST "/": it runs XapiValidateDiskPartition on F:, which formats a partition it
    # does not recognise)
    cmd('CWD /E')
    cmd('PWD')
    cmd('MKD evoxtest')
    cmd('CWD evoxtest')
    cmd('PWD')
    data_cmd('STOR hello.txt', b'Hello from server_test.py\r\n' * 40)
    data_cmd('LIST')
    cmd('RNFR hello.txt')
    cmd('RNTO renamed.txt')
    data_cmd('RETR renamed.txt')
    data_cmd('RETR missing.txt')
    cmd('DELE renamed.txt')
    cmd('CDUP')
    cmd('PWD')
    cmd('RMD evoxtest')
    cmd('CWD /nosuchdrive')
    cmd('HDDINFO')
    cmd('DUMPHDDSECTOR 0 1')
    cmd('QUIT')
    s.close()


if __name__ == '__main__':
    what = sys.argv[1] if len(sys.argv) > 1 else 'ftp'
    if what == 'ftp':
        ftp(int(sys.argv[2]) if len(sys.argv) > 2 else 2121)
