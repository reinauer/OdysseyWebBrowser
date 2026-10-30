#!/usr/bin/env python3
"""Stage and test Odyssey in a disposable native AmigaOS 3 RTG installation.

Requires a legally installed OS/Picasso96/AmiSSL system and native MUI classes.
This never changes the source installation or injects host keyboard input.
"""
import argparse
import base64
import hashlib
import http.server
import json
import os
import secrets
from pathlib import Path
import shutil
import socket
import socketserver
import struct
import ssl
import subprocess
import threading
import time
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parents[2]
PAGE = b'''<!doctype html><meta charset="utf-8"><title>Odyssey AmigaOS 3 native test</title>
<style>body{background:#eef4fa;color:#182b45;font:20px sans-serif;margin:32px}
#box{width:240px;height:80px;background:#246cb5;color:white;padding:16px}
canvas{border:2px solid #182b45} .pass{color:#166534}</style>
<h1>Odyssey on AmigaOS 3</h1><p id="unicode">Unicode: caf\xc3\xa9 \xce\xa9</p>
<div id="box">Native HTML and CSS</div><p><canvas id="canvas" width="160" height="80"></canvas></p>
<p id="status">Waiting for JavaScript...</p><a href="/second.html">Navigation test</a>
<script>
window.onload=function(){var websocketTest=false;var blackholePort=0;var httpsTest=false;var httpsPort=0;var untrustedPort=0;var networkTimeout=0;var navigationTest=false;var interactionTest=false;var storageToken='persisted';var result={phase:'initial'};try{
result.dom=document.getElementById('unicode').textContent==='Unicode: caf\\u00e9 \\u03a9';
result.layout=document.getElementById('box').getBoundingClientRect().width===272;
var c=document.getElementById('canvas').getContext('2d');
c.fillStyle='#246cb5';c.fillRect(0,0,160,80);c.fillStyle='#ffcc33';c.fillRect(20,20,120,40);
var p=c.getImageData(30,30,1,1).data;result.canvas=p[0]===255&&p[1]===204&&p[2]===51&&p[3]===255;
result.javascript=JSON.parse(JSON.stringify({value:21})).value*2===42;
result.storage=false;localStorage.setItem('odyssey-native',storageToken);result.storage=localStorage.getItem('odyssey-native')===storageToken;
}catch(e){result.error=String(e);}
var reported=false;var tlsStarted=false;function sendReport(){if(reported)return;reported=true;
var ok=result.dom&&result.layout&&result.canvas&&result.javascript&&result.storage&&(!websocketTest||(result.websocket&&result.httpDuringWebSocket&&result.websocketCancel&&result.httpAfterCancel))&&(!httpsTest||(result.httpsContent&&result.httpsScript&&result.wrongHostRejected&&result.untrustedRejected));
document.getElementById('status').textContent=ok?'PASS native browser checks':JSON.stringify(result);
document.getElementById('status').className=ok?'pass':'';
setTimeout(function(){var r=new XMLHttpRequest();r.open('GET','/report?data='+encodeURIComponent(JSON.stringify(result)),true);r.onload=function(){if(navigationTest&&ok)location.href='/second.html';else if(interactionTest&&ok)location.href='/interaction.html';};r.send();},1000);
}
function report(){if(!httpsTest){sendReport();return;}if(tlsStarted)return;tlsStarted=true;
function request(url,done){var r=new XMLHttpRequest();r.open('GET',url,true);r.timeout=networkTimeout;r.onload=function(){done(false,r.status,r.responseText);};r.onerror=function(){done(true,0,'');};r.ontimeout=function(){result.httpsError='timeout';done(false,0,'');};r.send();}
request('https://localhost:'+httpsPort+'/tls-trusted',function(error,status,text){result.httpsStatus=status;result.httpsNetworkError=error;try{var data=JSON.parse(text);result.httpsContent=!error&&status===200&&data.message==='trusted HTTPS'&&data.value*2===42;}catch(e){result.httpsContent=false;}
var script=document.createElement('script');script.crossOrigin='anonymous';script.src='https://localhost:'+httpsPort+'/tls-script.js';
var scriptDone=false;var scriptTimer=setTimeout(function(){finishScript(false);},networkTimeout);
function finishScript(ok){if(scriptDone)return;scriptDone=true;clearTimeout(scriptTimer);result.httpsScript=ok&&window.odysseyTLS&&window.odysseyTLS.value===42;
request('https://127.0.0.1:'+httpsPort+'/tls-wrong-host',function(error){result.wrongHostRejected=error;
request('https://localhost:'+untrustedPort+'/tls-untrusted',function(error){result.untrustedRejected=error;sendReport();});});}
script.onload=function(){finishScript(true);};script.onerror=function(){finishScript(false);};document.head.appendChild(script);
});}
if(websocketTest){result.websocket=false;result.websocketEvents=[];var wsStarted=Date.now();function wsEvent(event){result.websocketEvents.push({event:event,elapsedMs:Date.now()-wsStarted});}try{
var expected='Odyssey \\u03a9 native WebSocket echo 68k';
var pending=new WebSocket('wss://127.0.0.1:'+blackholePort+'/pending');
var cancelled=false;wsEvent('created pending TLS');
var timer=setTimeout(function(){result.websocketTimeout=true;result.websocketError='timeout';wsEvent('timeout');pending.close();ws.close();report();},networkTimeout);
function getMarker(path,done){var r=new XMLHttpRequest();r.open('GET',path,true);r.onload=function(){done(r.status===200&&r.responseText==='odyssey-http-marker');};r.onerror=function(){done(false);};r.send();}
function finish(){if(result.websocketCancel&&result.httpAfterCancel){clearTimeout(timer);report();}}
pending.onerror=function(){wsEvent('pending error');};
pending.onclose=function(){wsEvent('pending closed');result.websocketCancel=cancelled;finish();};
function cancelPending(){if(result.websocket&&result.httpDuringWebSocket){
result.pendingWasConnecting=pending.readyState===WebSocket.CONNECTING;
if(!result.pendingWasConnecting){result.websocketError='WSS did not remain pending';clearTimeout(timer);report();return;}
cancelled=true;wsEvent('cancel pending');pending.close();getMarker('/after-wss-cancel',function(ok){result.httpAfterCancel=ok;if(!ok){clearTimeout(timer);report();}else finish();});}}
getMarker('/while-wss-pending',function(ok){wsEvent('HTTP marker while pending');result.httpWhileTLSConnecting=pending.readyState===WebSocket.CONNECTING;result.httpDuringWebSocket=ok&&result.httpWhileTLSConnecting;if(!result.httpDuringWebSocket){clearTimeout(timer);report();}else cancelPending();});
var ws=new WebSocket('ws://'+location.host+'/websocket');
wsEvent('created echo');
ws.onopen=function(){wsEvent('echo opened');ws.send(expected);};
ws.onclose=function(){wsEvent('echo closed');};
ws.onmessage=function(event){wsEvent('echo received');result.websocket=event.data===expected;ws.close();if(!result.websocket){clearTimeout(timer);report();}else cancelPending();};
ws.onerror=function(){wsEvent('echo error');if(!result.websocketError)result.websocketError='connection error';clearTimeout(timer);report();};
}catch(e){result.websocketError=String(e);report();}}else report();
};</script>'''


def test_page(args):
    page = PAGE
    for key, value in {'websocketTest': args.websocket, 'blackholePort': args.blackhole_port,
                       'httpsTest': args.https, 'httpsPort': args.https_port,
                       'untrustedPort': args.untrusted_port, 'networkTimeout': args.network_timeout * 1000, 'navigationTest': args.navigation, 'interactionTest': args.interaction}.items():
        original = 'false' if isinstance(value, bool) else '0'
        page = page.replace(('var '+key+'='+original+';').encode(), ('var '+key+'='+json.dumps(value)+';').encode())
    return page.replace(b"var storageToken='persisted';", ('var storageToken='+json.dumps(args.storage_token)+';').encode())


def second_page(args):
    if not args.navigation:
        return b'<!doctype html><title>Second page</title><h1>Navigation completed</h1><a href="/">Back</a>'
    return ('''<!doctype html><meta charset="utf-8"><title>Odyssey persistence check</title>
<h1 id="status">Checking storage after navigation</h1><script>
window.onload=function(){var token=TOKEN;var reloaded=sessionStorage.getItem('odyssey-reload')===token;
var result={phase:reloaded?'reload':'navigation',storageAfterNavigation:false};
try{result.storageAfterNavigation=localStorage.getItem('odyssey-native')===token;}catch(e){result.error=String(e);}
document.getElementById('status').textContent=result.storageAfterNavigation?'PASS '+result.phase+' storage':'FAIL storage';
var request=new XMLHttpRequest();request.open('GET','/report?data='+encodeURIComponent(JSON.stringify(result)),true);
request.onload=function(){if(!reloaded&&result.storageAfterNavigation){sessionStorage.setItem('odyssey-reload',token);location.reload();}else if(INTERACTION&&result.storageAfterNavigation)location.href='/interaction.html';};request.send();
};</script>'''.replace('TOKEN', json.dumps(args.storage_token)).replace('INTERACTION', json.dumps(args.interaction))).encode('utf-8')


INTERACTION_PAGE = b'''<!doctype html><meta charset="utf-8"><title>Native input test</title>
<style>body{font:24px sans-serif;margin:32px;background:#eef4fa;color:#182b45}
input{font:24px sans-serif;width:300px;height:40px}button{font:24px sans-serif;margin-left:20px;height:48px}</style>
<h1>Native page input</h1><p>Click the field, type odyssey68k, then click Check.</p>
<input id="entry" autocomplete="off"><button id="check">Check</button><p id="status">Waiting for native input...</p>
<script>var field=document.getElementById('entry');var events=[];
['focus','keydown','input'].forEach(function(type){field.addEventListener(type,function(e){events.push({type:e.type,trusted:e.isTrusted,value:field.value});});});
document.getElementById('check').onclick=function(e){events.push({type:'click',trusted:e.isTrusted});
var result={phase:'interaction',value:field.value,events:events};
result.nativeInput=field.value==='odyssey68k'&&['focus','keydown','input','click'].every(function(type){return events.some(function(event){return event.type===type&&event.trusted===true;});});
document.getElementById('status').textContent=result.nativeInput?'PASS native input':JSON.stringify(result);
var request=new XMLHttpRequest();request.open('GET','/report?data='+encodeURIComponent(JSON.stringify(result)),true);request.send();};</script>'''


def prepare_tls(args, root, guest):
    directory = root/'tls'
    directory.mkdir()
    def openssl(*arguments):
        with (root/'tls-build.log').open('ab') as log:
            subprocess.run([str(args.openssl.resolve()), *arguments], cwd=directory,
                           stdout=log, stderr=log, check=True, timeout=60)
    openssl('req','-x509','-newkey','rsa:2048','-nodes','-keyout','ca-key.pem','-out','ca.pem',
            '-days','2','-subj','/CN=Odyssey native test CA','-addext','basicConstraints=critical,CA:TRUE',
            '-addext','keyUsage=critical,keyCertSign,cRLSign')
    openssl('req','-new','-newkey','rsa:2048','-nodes','-keyout','server-key.pem','-out','server.csr','-subj','/CN=localhost')
    (directory/'server.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n')
    openssl('x509','-req','-in','server.csr','-CA','ca.pem','-CAkey','ca-key.pem','-set_serial','1',
            '-out','server.pem','-days','2','-extfile','server.ext')
    openssl('req','-x509','-newkey','rsa:2048','-nodes','-keyout','untrusted-key.pem','-out','untrusted.pem',
            '-days','2','-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost')
    cert_hash = subprocess.check_output([str(args.openssl.resolve()), 'x509','-in',str(directory/'ca.pem'),
                                         '-noout','-subject_hash'], text=True).strip()
    trust = guest/'AmiSSL/Certs'
    trust.mkdir(exist_ok=True)
    index = 0
    while (trust/(cert_hash+'.'+str(index))).exists():
        index += 1
    shutil.copy2(directory/'ca.pem', trust/(cert_hash+'.'+str(index)))
    (guest/'browser-test-utc.txt').write_text(str(int(time.time()))+'\n')
    (root/'tls-inputs.json').write_text(json.dumps({'trusted_ca_sha256':digest(directory/'ca.pem'),
        'trusted_leaf_sha256':digest(directory/'server.pem'),'untrusted_leaf_sha256':digest(directory/'untrusted.pem'),
        'guest_ca_file':str((trust/(cert_hash+'.'+str(index))).relative_to(guest)),
        'clock_note':'Only the disposable guest timer.device clock is set; source installation and host clock are unchanged.'},indent=2)+'\n')


def websocket_echo(handler, events, timeout=60):
    """Bounded RFC 6455 text echo for this fixture; no external services."""
    key = handler.headers.get('Sec-WebSocket-Key', '')
    try:
        valid_key = len(base64.b64decode(key, validate=True)) == 16
    except ValueError:
        valid_key = False
    if (not valid_key or handler.headers.get('Upgrade', '').lower() != 'websocket'
        or 'upgrade' not in [item.strip().lower() for item in handler.headers.get('Connection', '').split(',')]
        or handler.headers.get('Sec-WebSocket-Version') != '13'):
        handler.send_error(400, 'Invalid WebSocket handshake')
        return
    accept = base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode('ascii')).digest()).decode('ascii')
    handler.send_response(101, 'Switching Protocols')
    handler.send_header('Upgrade', 'websocket')
    handler.send_header('Connection', 'Upgrade')
    handler.send_header('Sec-WebSocket-Accept', accept)
    handler.end_headers()
    handler.wfile.flush()
    handler.close_connection = True
    handler.connection.settimeout(timeout)
    events.append({'event': 'handshake', 'time': time.time(), 'monotonic': time.monotonic()})

    def read_exact(length, stage):
        data = handler.rfile.read(length)
        if len(data) != length:
            events.append({'event':'incomplete frame', 'stage':stage,
                'expected':length, 'received':len(data), 'prefix_hex':data[:32].hex(),
                'monotonic':time.monotonic()})
            raise EOFError('incomplete WebSocket '+stage)
        return data

    def send(opcode, payload):
        prefix = bytes((0x80 | opcode, len(payload))) if len(payload) < 126 else bytes((0x80 | opcode, 126))+struct.pack('!H', len(payload))
        handler.wfile.write(prefix+payload)
        handler.wfile.flush()

    try:
        for unused in range(16):
            first, second = read_exact(2, 'header')
            events.append({'event':'frame header', 'header_hex':bytes((first, second)).hex(), 'monotonic':time.monotonic()})
            opcode, length = first & 15, second & 127
            if length == 126:
                length = struct.unpack('!H', read_exact(2, 'extended16 length'))[0]
            elif length == 127:
                length = struct.unpack('!Q', read_exact(8, 'extended64 length'))[0]
            # Browser text payload is short and unfragmented. Reject unsupported
            # extensions/fragmentation explicitly instead of misparsing them.
            if not first & 0x80 or first & 0x70 or not second & 0x80 or length > 65535 or (opcode >= 8 and length > 125):
                send(8, struct.pack('!H', 1002))
                raise ValueError('unsupported or invalid WebSocket frame')
            mask = read_exact(4, 'mask')
            payload = bytes(value ^ mask[index % 4] for index, value in enumerate(read_exact(length, 'payload')))
            if opcode == 8:
                send(8, payload)
                events.append({'event': 'close', 'time': time.time(), 'monotonic': time.monotonic()})
                return
            if opcode == 9:
                send(10, payload)
            elif opcode == 1:
                events.append({'event': 'echo', 'text': payload.decode('utf-8'), 'time': time.time(), 'monotonic': time.monotonic()})
                send(1, payload)
            elif opcode != 10:
                send(8, struct.pack('!H', 1002))
                raise ValueError('unsupported WebSocket opcode')
        raise ValueError('fixture WebSocket frame limit exceeded')
    except (OSError, EOFError, ValueError) as error:
        events.append({'event': 'error', 'message': str(error), 'time': time.time(), 'monotonic': time.monotonic()})


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def native_hunk(path):
    if path.read_bytes()[:4] != b'\x00\x00\x03\xf3':
        raise RuntimeError('expected a native m68k HUNK binary: '+str(path))


def copy_tree(source, destination):
    shutil.copytree(source, destination, dirs_exist_ok=True)


def omit_blocking_desktop_preferences(root, guest):
    """Preserve isolated backups of the observed inherited desktop blockers."""
    omissions = []
    def omit(relative, reason):
        path = guest/relative
        if not path.is_file():
            return
        backup = root/'omitted-desktop-files'/relative
        backup.parent.mkdir(parents=True, exist_ok=True)
        omissions.append({'path':relative,'sha256':digest(path),'reason':reason})
        shutil.move(path, backup)
    # Keep the installed wallpaper when the normal datatype initialization
    # command is present. Without it IPrefs cannot load the existing image.
    if not (guest/'C/AddDataTypes').is_file():
        omit('Prefs/Env-Archive/Sys/WBPattern.prefs',
             'No AddDataTypes initializer is installed for the inherited wallpaper')
    manager = guest/'Prefs/Env-Archive/ToolManager.prefs'
    if manager.is_file() and b'work:' in manager.read_bytes().lower():
        # The inherited auto-dock loads Work: images, but this fixture mounts
        # only SYS: and optional DH1:. Preserve every other startup utility.
        for name in ('ToolManager','ToolManager.info'):
            omit('WBStartup/'+name,
                 'ToolManager auto-dock references unavailable Work: assets')
    return omissions


def stage(args):
    root, source = args.output.resolve(), args.system.resolve()
    if root == source or source in root.parents or root in source.parents:
        raise RuntimeError('output and source system must be separate trees')
    required = [source/'C'/name for name in ('Assign', 'Copy', 'MakeDir', 'Wait', 'LoadMonDrvs', 'LoadWB', 'IPrefs')]
    required += [args.rom, args.emulator, args.cc, args.mui_library,
                 Path(__file__), ROOT/'Tools/AmigaOS3/BrowserFixtureTest.c',
                 source/'Devs/DataTypes/PNG',
                 source/'Classes/DataTypes/png.datatype',
                 source/'Classes/DataTypes/picture.datatype',
                 args.distribution/'Resource/back.png',
                 args.sdk/'share/amigaos3/Conf/fontconfig/fonts.conf',
                 args.sdk/'share/amigaos3/Fonts/NotoSans-Regular.ttf']
    if args.binary:
        required.append(args.binary)
    if args.calltips:
        required.append(args.calltips)
    if args.zune_library:
        required.append(args.zune_library)
    if args.https:
        if not args.openssl:
            raise RuntimeError('--https requires --openssl or openssl on PATH')
        required.append(args.openssl)
    if args.data_hdf:
        required.append(args.data_hdf)
        if not args.xdftool:
            raise RuntimeError('--data-hdf requires --xdftool or xdftool on PATH')
        required.append(args.xdftool)
    for path in required:
        if not path.is_file():
            raise RuntimeError('missing required input: '+str(path))
    if not (source/'AmiSSL').is_dir():
        raise RuntimeError('system must contain the installed AmiSSL runtime')
    if args.data_hdf:
        with args.data_hdf.open('rb') as image:
            if image.read(4) != b'DOS\x07':
                raise RuntimeError('--data-hdf requires a raw DOS7 long-filename FFS image')
        if args.data_hdf.stat().st_size < 256 * 1024 * 1024:
            raise RuntimeError('browser data hardfile must be at least 256 MiB')
    root.mkdir(parents=True, exist_ok=False)
    guest = root/'system'
    shutil.copytree(source, guest)
    omissions = omit_blocking_desktop_preferences(root, guest)
    for name in ('fixture-output.txt', 'browser-output.txt'):
        (guest/name).unlink(missing_ok=True)
    if args.https:
        prepare_tls(args, root, guest)
    browser = guest/'Odyssey'
    copy_tree(args.distribution, browser)
    copy_tree(args.sdk/'share/amigaos3', browser)
    # Preserve Dist resources while selecting the tested relocatable configuration.
    (browser/'Conf/font').mkdir(parents=True, exist_ok=True)
    shutil.copy2(browser/'Conf/fontconfig/fonts.conf', browser/'Conf/font/fonts.conf')
    if args.binary:
        native_hunk(args.binary)
        shutil.copy2(args.binary, browser/'OWB')
    native_hunk(args.mui_library)
    shutil.copy2(args.mui_library, guest/'Libs/muimaster.library')
    if args.zune_library:
        native_hunk(args.zune_library)
        shutil.copy2(args.zune_library, guest/'Libs/zunemaster.library')
    if args.mui_classes:
        for path in args.mui_classes.iterdir():
            if path.is_file() and path.suffix.lower() in ('.mcc', '.mcp', '.mui'):
                native_hunk(path)
                shutil.copy2(path, guest/'Libs/MUI'/path.name)
    if args.calltips:
        native_hunk(args.calltips)
        shutil.copy2(args.calltips, guest/'Libs/MUI/Calltips.mcc')
    for name in ('Listtree.mcc', 'Calltips.mcc'):
        path = guest/'Libs/MUI'/name
        if not path.is_file():
            raise RuntimeError('required MUI superclass missing: '+str(path))
        native_hunk(path)
    command = [str(args.cc.resolve()), '-m68020', '-O2',
               str(ROOT/'Tools/AmigaOS3/BrowserFixtureTest.c'), '-o',
               str(guest/'browser-fixture-test'), '-lamiga']
    with (root/'fixture-build.log').open('w') as log:
        subprocess.run(command, check=True, stdout=log, stderr=log)
    for path in guest.rglob('*'):
        if path.is_file():
            path.chmod(path.stat().st_mode | 0o100)
    # Retain installed settings, including fonts, instead of discarding ENV.
    if args.disable_topsites_screenshots:
        (guest/'Prefs/Env-Archive/OWB_DISABLE_TOPSITES_SCREENSHOTS').write_text('1')
    if args.trace_streams:
        (guest/'Prefs/Env-Archive/OWB_CURL_STREAM_TRACE').write_text('SYS:browser-trace.txt')
    datatype_setup = 'C:AddDataTypes REFRESH QUIET\n' if (guest/'C/AddDataTypes').is_file() else ''
    startup = ('FailAt 21\nC:MakeDir RAM:T RAM:ENV\nC:Assign T: RAM:T\n'
               'C:Copy SYS:Prefs/Env-Archive RAM:ENV ALL QUIET\n'
               'C:Assign ENV: RAM:ENV\nC:Assign MUI: SYS:\n'
               'C:Assign LIBS: SYS:Classes ADD\nC:Assign AmiSSL: SYS:AmiSSL\n'
               + datatype_setup + 'C:LoadMonDrvs\nStack 1048576\n'
               'SYS:browser-fixture-test --screen-mode >SYS:fixture-output.txt\n'
               'If WARN\nEcho "FIXTURE SCREEN FAILED" >>SYS:fixture-output.txt\nQuit 20\nEndIf\n'
               'Run >NIL: C:IPrefs\nC:Wait 2 SEC\nC:LoadWB\nC:Wait 2 SEC\n'
               'SYS:browser-fixture-test >>SYS:fixture-output.txt\n'
               'If WARN\nEcho "FIXTURE CHECK FAILED" >>SYS:fixture-output.txt\nQuit 20\nEndIf\n')
    if args.data_hdf:
        startup += ('SYS:browser-fixture-test --disk >>SYS:fixture-output.txt\n'
                    'If WARN\nEcho "FIXTURE DISK FAILED" >>SYS:fixture-output.txt\nQuit 20\nEndIf\n'
                    'Echo "Browser files staged on native DH1:Odyssey" >>SYS:fixture-output.txt\n')
    if args.https:
        startup += ('SYS:browser-fixture-test --clock >>SYS:fixture-output.txt\n'
                    'If WARN\nEcho "FIXTURE CLOCK FAILED" >>SYS:fixture-output.txt\nQuit 20\nEndIf\n')
    if args.fixture_only:
        startup += 'Echo FIXTURE_DONE >>SYS:fixture-output.txt\nEndCLI >NIL:\n'
    else:
        startup += (f'C:Wait {args.startup_delay} SEC\n' if args.startup_delay else '')
        browser_volume = 'DH1:' if args.data_hdf else 'SYS:'
        startup += (f'{browser_volume}Odyssey/OWB "http://127.0.0.1:{args.http_port}/" >SYS:browser-output.txt\n'
                    'Echo "RETURN=$RC" >>SYS:browser-output.txt\n'
                    'Echo BROWSER_EXIT >>SYS:browser-output.txt\n')
    (guest/'S/Startup-Sequence').write_text(startup)
    config = root/'run.uae'
    disk_config = ''
    if args.data_hdf:
        disk = root/'data.hdf'
        shutil.copy2(args.data_hdf, disk)
        with (root/'hardfile-stage.log').open('w') as log:
            subprocess.run([str(args.xdftool.resolve()), str(disk), 'write', str(browser), 'Odyssey'],
                           check=True, stdout=log, stderr=log)
        disk_config = f'hardfile2=rw,DH1:{disk},32,1,2,512,-128,,\n'
    config.write_text(
        'config_description=Odyssey native browser RTG test\nuse_gui=no\n'
        f'kickstart_rom_file={args.rom.resolve()}\n'
        'cpu_model=68020\ncpu_24bit_addressing=false\ncpu_speed=max\n'
        'cpu_compatible=false\nchipset=aga\nchipmem_size=4\n'
        f'z3mem_size={args.z3_memory}\nfastmem_size=8\nsound_output=none\n'
        'bsdsocket_emu=true\ngfxcard_size=16\ngfxcard_type=ZorroIII\n'
        'gfx_width_windowed=800\ngfx_height_windowed=600\n'
        f'filesystem2=rw,DH0:System:{guest},0\nuaehf0=dir,rw,DH0:System:{guest},0\n'
        + disk_config + f'debugging_features=gdbserver\ngdb_port={args.gdb_port}\n')
    inputs = {'command': command, 'desktop_omissions':omissions,
              'arguments': {key: str(value) if isinstance(value, Path) else value
                                              for key, value in vars(args).items()},
              'inputs': {str(path.resolve()): digest(path) for path in required},
              'guest_files': {str(path.relative_to(guest)): digest(path)
                              for path in sorted(guest.rglob('*')) if path.is_file()},
              'config_sha256': digest(config), 'page_sha256': hashlib.sha256(test_page(args)).hexdigest(),
              'second_page_sha256': hashlib.sha256(second_page(args)).hexdigest(),
              'interaction_page_sha256': hashlib.sha256(INTERACTION_PAGE).hexdigest() if args.interaction else None}
    (root/'inputs.json').write_text(json.dumps(inputs, indent=2)+'\n')
    (root/'test-page.html').write_bytes(test_page(args))
    (root/'second-page.html').write_bytes(second_page(args))
    if args.interaction:
        (root/'interaction-page.html').write_bytes(INTERACTION_PAGE)
    return root, guest, config


def run(args, root, guest, config):
    reports, requests, websocket_events, blackhole_events, tls_events = [], [], [], [], []
    stop_blackhole = threading.Event()
    class Blackhole(socketserver.BaseRequestHandler):
        def handle(self):
            blackhole_events.append({'event': 'accepted', 'monotonic': time.monotonic()})
            self.request.settimeout(1)
            while not stop_blackhole.is_set():
                try:
                    data = self.request.recv(65536)
                    if not data:
                        blackhole_events.append({'event': 'closed', 'monotonic': time.monotonic()})
                        return
                    blackhole_events.append({'event': 'received', 'bytes': len(data), 'monotonic': time.monotonic()})
                except socket.timeout:
                    continue
                except OSError:
                    blackhole_events.append({'event': 'closed', 'monotonic': time.monotonic()})
                    return
    class BlackholeServer(socketserver.ThreadingTCPServer):
        daemon_threads = True
    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'
        def handle(self):
            try:
                super().handle()
            except (ConnectionResetError, BrokenPipeError, ssl.SSLError):
                pass
        def do_GET(self):
            requests.append({'port':self.server.server_port, 'path':self.path, 'monotonic':time.monotonic()})
            url = urlsplit(self.path)
            if url.path == '/websocket' and args.websocket:
                websocket_echo(self, websocket_events, args.network_timeout)
                return
            if url.path == '/report':
                try:
                    report = json.loads(parse_qs(url.query)['data'][0])
                except (KeyError, ValueError):
                    report = {'error': 'malformed report'}
                report['hostReceivedMonotonic'] = time.monotonic()
                reports.append(report)
                print('Browser report:', json.dumps(report), flush=True)
                with (root/'reports.jsonl').open('a') as log:
                    log.write(json.dumps(report)+'\n')
                (root/'events-at-report.json').write_text(json.dumps({
                    'host_monotonic':time.monotonic(), 'reports':list(reports),
                    'requests':list(requests), 'websocket_events':list(websocket_events),
                    'blackhole_events':list(blackhole_events), 'tls_events':list(tls_events),
                }, indent=2)+'\n')
                body = b'acknowledged'
            elif url.path == '/second.html':
                body = second_page(args)
            elif url.path == '/interaction.html' and args.interaction:
                body = INTERACTION_PAGE
            elif url.path == '/tls-trusted':
                body = b'{"message":"trusted HTTPS","value":21}'
            elif url.path == '/tls-script.js':
                body = b'window.odysseyTLS={value:6*7};'
            elif url.path in ('/tls-wrong-host','/tls-untrusted'):
                body = b'FAIL: a certificate-negative request reached HTTP'
            elif url.path in ('/while-wss-pending', '/after-wss-cancel'):
                body = b'odyssey-http-marker'
            else:
                body = test_page(args)
            self.send_response(200)
            self.send_header('Content-Type', 'application/javascript' if url.path == '/tls-script.js' else 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            if url.path.startswith('/tls-'):
                # GETs and anonymous classic-script loading need no preflight.
                # Explicit CORS prevents a certificate failure false positive.
                self.send_header('Access-Control-Allow-Origin', f'http://127.0.0.1:{args.http_port}')
                self.send_header('Vary', 'Origin')
            self.end_headers()
            self.wfile.write(body)
        def log_message(self, *unused):
            pass
    class TLSHTTPServer(http.server.ThreadingHTTPServer):
        daemon_threads = True
        def get_request(self):
            connection, address = super().get_request()
            connection.settimeout(args.network_timeout)
            started = time.monotonic()
            try:
                secured = self.context.wrap_socket(connection, server_side=True)
                tls_events.append({'port':self.server_port,'event':'handshake','seconds':round(time.monotonic()-started,3)})
                return secured, address
            except (OSError, ssl.SSLError) as error:
                tls_events.append({'port':self.server_port,'event':'rejected','error':str(error),'seconds':round(time.monotonic()-started,3)})
                connection.close()
                raise
    with socket.socket() as check:
        check.bind(('127.0.0.1', args.gdb_port))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', args.http_port), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    blackhole = None
    tls_servers = []
    if args.https:
        try:
            for port, certificate, key in ((args.https_port,'server.pem','server-key.pem'),
                                           (args.untrusted_port,'untrusted.pem','untrusted-key.pem')):
                tls_server = TLSHTTPServer(('127.0.0.1',port), Handler)
                tls_servers.append(tls_server)
                tls_server.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                tls_server.context.load_cert_chain(root/'tls'/certificate, root/'tls'/key)
                threading.Thread(target=tls_server.serve_forever,daemon=True).start()
        except Exception:
            for tls_server in tls_servers:
                tls_server.server_close()
            server.shutdown()
            server.server_close()
            raise
    if args.websocket:
        try:
            blackhole = BlackholeServer(('127.0.0.1', args.blackhole_port), Blackhole)
        except Exception:
            server.shutdown()
            server.server_close()
            raise
        threading.Thread(target=blackhole.serve_forever, daemon=True).start()
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    status = {'passed': False, 'boundary': 'native guest fixture and page assertions; screenshots require visual inspection'}
    try:
        with (root/'emulator.log').open('w') as log:
            process = subprocess.Popen([str(args.emulator.resolve()), '-f', str(config)], env=env, stdout=log, stderr=log)
            try:
                deadline = time.monotonic()+args.timeout
                while time.monotonic() < deadline:
                    fixture = (guest/'fixture-output.txt').read_text(errors='replace') if (guest/'fixture-output.txt').exists() else ''
                    if 'FAILED' in fixture:
                        raise RuntimeError('guest fixture failed; see fixture-output.txt')
                    if args.fixture_only and 'FIXTURE_DONE' in fixture:
                        status['passed'] = 'Browser fixture: 0 failures' in fixture
                        break
                    if reports:
                        checks = ('dom','layout','canvas','javascript','storage') + (('websocket','httpDuringWebSocket','pendingWasConnecting','websocketCancel','httpAfterCancel') if args.websocket else ())
                        if args.https:
                            checks += ('httpsContent','httpsScript','wrongHostRejected','untrustedRejected')
                        initial = next((report for report in reports if report.get('phase') == 'initial'), {})
                        status['passed'] = all(initial.get(name) is True for name in checks)
                        if args.websocket:
                            status['passed'] = status['passed'] and any(event['event'] == 'received' for event in blackhole_events) and any(event['event'] == 'closed' for event in blackhole_events)
                        if args.https:
                            status['passed'] = status['passed'] and not any(urlsplit(request['path']).path in ('/tls-wrong-host','/tls-untrusted') for request in requests)
                            status['passed'] = status['passed'] and any(event['port'] == args.untrusted_port for event in tls_events)
                            status['passed'] = status['passed'] and all(any(request['port'] == args.https_port and urlsplit(request['path']).path == path for request in requests) for path in ('/tls-trusted','/tls-script.js'))
                        if not status['passed']:
                            break
                        navigation = next((report for report in reports if report.get('phase') == 'navigation'), None)
                        reload = next((report for report in reports if report.get('phase') == 'reload'), None)
                        if navigation and navigation.get('storageAfterNavigation') is not True:
                            status['passed'] = False
                            break
                        if args.navigation and navigation and reload:
                            status['passed'] = reload.get('storageAfterNavigation') is True
                        required_reports_ready = not args.navigation or bool(navigation and reload)
                        if args.interaction and status['passed']:
                            interaction = next((report for report in reports if report.get('phase') == 'interaction'), None)
                            required_reports_ready = required_reports_ready and interaction is not None
                            if interaction:
                                status['passed'] = interaction.get('nativeInput') is True
                        if not status['passed'] or required_reports_ready:
                            break
                    if process.poll() is not None:
                        raise RuntimeError('emulator exited before completion')
                    if (guest/'browser-output.txt').exists() and 'BROWSER_EXIT' in (guest/'browser-output.txt').read_text(errors='replace'):
                        raise RuntimeError('browser exited before reporting page checks')
                    time.sleep(1)
                else:
                    raise RuntimeError('native browser timed out; inspect logs and GDB')
                if args.gdb:
                    with (root/'gdb-screenshot.log').open('w') as log:
                        capture = subprocess.run([str(args.gdb.resolve()), '--batch',
                            '-ex', f'target remote 127.0.0.1:{args.gdb_port}',
                            '-ex', 'monitor screenshot '+str(root/'screen.png'), '-ex', 'detach'],
                            stdout=log, stderr=log, timeout=30)
                    status['screenshot'] = capture.returncode == 0 and (root/'screen.png').is_file()
                    if not status['screenshot']:
                        raise RuntimeError('GDB screenshot failed')
                if args.hold:
                    print('Holding guest for GDB inspection for', args.hold, 'seconds', flush=True)
                    hold_until = time.monotonic()+args.hold
                    while time.monotonic() < hold_until and process.poll() is None:
                        output = guest/'browser-output.txt'
                        text = output.read_text(errors='replace') if output.exists() else ''
                        if 'BROWSER_EXIT' in text.splitlines():
                            status['browser_exit'] = True
                            status['browser_return'] = next((line.split('=',1)[1] for line in reversed(text.splitlines()) if line.startswith('RETURN=')), None)
                            print('Browser exited normally during hold; RETURN='+str(status['browser_return']), flush=True)
                            break
                        time.sleep(1)
            finally:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    except Exception as error:
        status['passed'] = False
        status['error'] = str(error)
        raise
    finally:
        server.shutdown()
        server.server_close()
        stop_blackhole.set()
        if blackhole:
            blackhole.shutdown()
            blackhole.server_close()
        for tls_server in tls_servers:
            tls_server.shutdown()
            tls_server.server_close()
        status.update(reports=reports, requests=requests, websocket_events=websocket_events, blackhole_events=blackhole_events, tls_events=tls_events)
        for name in ('fixture-output.txt','browser-output.txt'):
            if (guest/name).exists():
                shutil.copy2(guest/name, root/name)
        (root/'result.json').write_text(json.dumps(status, indent=2)+'\n')
    if not status['passed']:
        raise RuntimeError('native page assertions failed')
    print('PASS native fixture' if args.fixture_only else 'PASS native browser page assertions')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('system', 'rom', 'emulator', 'output', 'mui-library'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--data-hdf', type=Path, help='clone a >=256 MiB raw DOS7 FFS hardfile and run from DH1:Odyssey')
    parser.add_argument('--xdftool', type=Path, default=shutil.which('xdftool'), help='amitools tool used to populate only the cloned data image')
    parser.add_argument('--mui-classes', type=Path)
    parser.add_argument('--zune-library', type=Path, help='matching zunemaster.library required by native Zune MCCs')
    parser.add_argument('--calltips', type=Path)
    parser.add_argument('--sdk', type=Path, default=ROOT/'build/amigaos3-sdk')
    parser.add_argument('--distribution', type=Path, default=ROOT/'Dist')
    parser.add_argument('--cc', type=Path, default=Path('/opt/amiga-16.2/bin/m68k-amigaos-gcc'))
    parser.add_argument('--gdb', type=Path, help='optional GDB executable for an RSP screenshot')
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--fixture-only', action='store_true')
    parser.add_argument('--websocket', action='store_true', help='require a browser WebSocket upgrade and Unicode text echo')
    parser.add_argument('--trace-streams', action='store_true', help='enable native connect/send/receive diagnostics in the copied guest')
    parser.add_argument('--disable-topsites-screenshots', action='store_true', help='diagnostic A/B: disable synchronous TopSites screenshots in the copied guest')
    parser.add_argument('--https', action='store_true', help='require trusted HTTPS content/script and certificate-negative requests')
    parser.add_argument('--navigation', action='store_true', help='navigate and reload a second page, preserving localStorage on DOS7')
    parser.add_argument('--interaction', action='store_true', help='require native mouse focus, typing odyssey68k, and clicking Check on the final page')
    parser.add_argument('--openssl', type=Path, default=shutil.which('openssl'))
    parser.add_argument('--gdb-port', type=int, default=2350)
    parser.add_argument('--http-port', type=int, default=18788)
    parser.add_argument('--blackhole-port', type=int, default=18789, help='TCP listener which deliberately leaves TLS pending for cancellation testing')
    parser.add_argument('--https-port', type=int, default=18444)
    parser.add_argument('--untrusted-port', type=int, default=18445)
    parser.add_argument('--z3-memory', type=int, choices=(128,256,512), default=512)
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--network-timeout', type=int, default=60, help='seconds allowed for HTTPS, WebSocket echo/cancel and host TLS/WS operations')
    parser.add_argument('--startup-delay', type=int, default=0)
    parser.add_argument('--hold', type=int, default=0, help='seconds for optional GDB inspection after checks')
    args = parser.parse_args()
    args.storage_token = secrets.token_hex(16)
    if not args.binary and not (args.prepare_only or args.fixture_only):
        parser.error('--binary is required for a browser run')
    ports = (args.http_port, args.gdb_port, args.blackhole_port) if args.websocket else (args.http_port, args.gdb_port)
    if args.https:
        ports += (args.https_port, args.untrusted_port)
    if args.navigation and not args.data_hdf:
        parser.error('--navigation requires --data-hdf for native filesystem storage')
    if min(args.timeout, args.network_timeout) <= 0 or min(args.hold, args.startup_delay) < 0 or any(not 0 < port <= 65535 for port in ports) or len(set(ports)) != len(ports):
        parser.error('invalid timing or port argument')
    root, guest, config = stage(args)
    print('Prepared', root, flush=True)
    if not args.prepare_only:
        run(args, root, guest, config)


if __name__ == '__main__':
    main()
