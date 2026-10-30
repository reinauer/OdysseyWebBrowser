#!/usr/bin/env python3
"""Verify native curl/AmiSSL HTTP, TLS trust and worker lifetimes in WinUAE.

Only disposable guest files and its clock are changed. The test uses a local
short-lived certificate, with verification enabled for every TLS request.
"""
import argparse
import hashlib
import http.server
import json
from pathlib import Path
import shutil
import ssl
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('system', 'rom', 'emulator', 'output'):
        p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--binary', type=Path, default=ROOT/'build/amigaos3-deps-curl/curl-test')
    p.add_argument('--gdb-port', type=int, default=2346)
    p.add_argument('--timeout', type=int, default=240)
    args = p.parse_args()
    output = args.output.resolve()
    source = args.system.resolve()
    if source == output or source in output.parents or output in source.parents:
        p.error('source and output must be separate directory trees')
    if not (source/'AmiSSL').is_dir():
        p.error('the guest fixture must contain its installed AmiSSL runtime')
    output.mkdir(parents=True, exist_ok=False)
    fixture = output/'fixture'
    shutil.copytree(source, fixture)
    cert, key = output/'ca.pem', output/'server-key.pem'
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-keyout', str(key), '-out', str(cert), '-days', '2',
                    '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
    shutil.copy2(cert, fixture/'curl-test-ca.pem')
    host_utc = int(time.time())
    (fixture/'curl-test-utc.txt').write_text(str(host_utc)+'\n')
    requests = []

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def handle(self):
            try:
                super().handle()
            except (ConnectionResetError, BrokenPipeError):
                # Rejected test certificates intentionally abort TLS before HTTP.
                pass

        def do_GET(self):
            requests.append({'port': self.server.server_port, 'path': self.path})
            body = b'odyssey-amigaos3-curl\n'
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Content-Type', 'text/plain')
            self.send_header('Connection', 'close')
            self.close_connection = True
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *unused):
            pass

    class Server(http.server.ThreadingHTTPServer):
        daemon_threads = True

        def get_request(self):
            connection, address = super().get_request()
            connection.settimeout(10)
            if self.server_port == 18443:
                try:
                    connection = context.wrap_socket(connection, server_side=True)
                except Exception:
                    connection.close()
                    raise
            return connection, address

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    servers = []
    try:
        for port in (18787, 18443):
            server = Server(('127.0.0.1', port), Handler)
            servers.append(server)
            threading.Thread(target=server.serve_forever, daemon=True).start()
        command = ['python3', str(ROOT/'Tools/AmigaOS3/run-platform-test.py'),
                   '--system', str(fixture), '--rom', str(args.rom.resolve()),
                   '--emulator', str(args.emulator.resolve()), '--binary', str(args.binary.resolve()),
                   '--output', str(output/'guest'), '--gdb-port', str(args.gdb_port),
                   '--timeout', str(args.timeout), '--bsdsocket',
                   '--success-marker', 'curl native: 0 failures']
        result = subprocess.run(command, timeout=args.timeout+30)
        manifest = {'host_utc': host_utc, 'certificate_sha256': digest(cert),
                    'probe_sha256': digest(args.binary), 'requests': requests,
                    'returncode': result.returncode, 'command': command,
                    'clock_note': 'Guest output records original UTC and locale offset before fixture-only synchronization.'}
        (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        return result.returncode
    finally:
        for server in servers:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    raise SystemExit(main())
