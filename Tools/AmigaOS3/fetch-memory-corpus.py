#!/usr/bin/env python3
"""Snapshot public pages and their subresources for repeatable browsing tests.

Snapshots are third-party content: keep them in ignored build storage. Pages
run from run-browser-test.py's loopback server, without any live connection.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import time
import urllib.error
from urllib.parse import urljoin, urlsplit
import urllib.request

AGENT = 'Mozilla/5.0 (compatible; OdysseyAmigaOS3MemoryCorpus/1.0)'
PAGES = (
    ('text-news', 'https://text.npr.org/'),
    ('link-list', 'https://news.ycombinator.com/'),
    ('aminet', 'https://aminet.net/'),
    ('amiga-news', 'https://www.amiga-news.de/en/'),
    ('wikipedia', 'https://en.wikipedia.org/wiki/Amiga'),
    ('github', 'https://github.com/AmigaLabs/OdysseyWebBrowser'),
)
ASSET = re.compile(r'''(\s(?:src|href)\s*=\s*)(["'])(.*?)\2''', re.I | re.S)
CSS_URL = re.compile(r'''url\(\s*(["']?)(?!data:)([^"')]+)\1\s*\)''', re.I)
SUBRESOURCE_TAG = re.compile(r'<(?:img|script|link)\b[^>]*>', re.I | re.S)


def download(url, timeout):
    request = urllib.request.Request(url, headers={'User-Agent': AGENT, 'Accept': '*/*'})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read(), response.headers.get_content_type(), response.geturl()


class Corpus:
    def __init__(self, output, args):
        self.output, self.args, self.assets, self.failures = output, args, {}, []
        (output/'assets').mkdir(parents=True)

    def asset(self, url, depth=0):
        """Return the local path of a stored subresource, or None if unavailable."""
        if url in self.assets:
            return '/corpus/assets/'+self.assets[url]['file']
        if len(self.assets) >= self.args.max_assets:
            return None
        try:
            body, content_type, final = download(url, self.args.timeout)
        except (OSError, urllib.error.URLError, ValueError) as error:
            self.failures.append({'url': url, 'error': str(error)})
            return None
        if len(body) > self.args.max_asset_bytes:
            self.failures.append({'url': url, 'error': 'exceeds size limit', 'bytes': len(body)})
            return None
        if content_type == 'text/css' and depth < 2:
            body = self.rewrite_css(body, final, depth)
        name = hashlib.sha256(url.encode()).hexdigest()[:16]
        (self.output/'assets'/name).write_bytes(body)
        self.assets[url] = {'file': name, 'type': content_type, 'bytes': len(body)}
        time.sleep(self.args.delay)
        return '/corpus/assets/'+name

    def rewrite_css(self, body, base, depth):
        def replace(match):
            local = self.asset(urljoin(base, match.group(2).strip()), depth + 1)
            return 'url('+(local or 'about:blank')+')'
        return CSS_URL.sub(replace, body.decode('utf-8', 'replace')).encode('utf-8')

    def rewrite_tag(self, match, base):
        tag = re.sub(r'\s(?:srcset|integrity|crossorigin|imagesrcset)\s*=\s*(["\']).*?\1', '', match.group(0), flags=re.I | re.S)
        if tag[:5].lower() == '<link' and not re.search(r'rel\s*=\s*["\']?stylesheet', tag, re.I):
            # Preloads, icons and preconnects to other hosts would reach the
            # live site over TLS during a run; GitHub opened five connections.
            if re.search(r'''\shref\s*=\s*["']?\s*(?:https?:)?//''', tag, re.I):
                return ''
            return tag
        def replace(attribute):
            target = attribute.group(3).strip()
            if target.startswith(('data:', '#', 'javascript:')):
                return attribute.group(0)
            local = self.asset(urljoin(base, target))
            return attribute.group(1)+'"'+(local or 'about:blank')+'"'
        return ASSET.sub(replace, tag)

    def page(self, name, url):
        body, unused, final = download(url, self.args.timeout)
        text = body.decode('utf-8', 'replace')
        # Reports and rewritten assets need the snapshot's own origin.
        text = re.sub(r'<meta[^>]+http-equiv\s*=\s*["\']?content-security-policy[^>]*>', '', text, flags=re.I)
        text = re.sub(r'<base\b[^>]*>', '', text, flags=re.I)
        text = SUBRESOURCE_TAG.sub(lambda match: self.rewrite_tag(match, final), text)
        page = self.output/(name+'.html')
        page.write_text(text, encoding='utf-8')
        return {'name': name, 'url': url, 'final_url': final, 'file': page.name,
                'bytes': page.stat().st_size,
                'sha256': hashlib.sha256(page.read_bytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=30)
    parser.add_argument('--delay', type=float, default=0.1, help='seconds between subresource requests')
    parser.add_argument('--max-assets', type=int, default=600)
    parser.add_argument('--max-asset-bytes', type=int, default=8*1024*1024)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output already exists: '+str(args.output))
    corpus = Corpus(args.output, args)
    pages, skipped = [], []
    for name, url in PAGES:
        before = sum(entry['bytes'] for entry in corpus.assets.values())
        count = len(corpus.assets)
        try:
            page = corpus.page(name, url)
        except (OSError, urllib.error.URLError, ValueError) as error:
            skipped.append({'name': name, 'url': url, 'error': str(error)})
            continue
        page['new_assets'] = len(corpus.assets) - count
        page['new_asset_bytes'] = sum(entry['bytes'] for entry in corpus.assets.values()) - before
        pages.append(page)
        print(name, page['bytes'], 'bytes +', page['new_assets'], 'assets', page['new_asset_bytes'], 'bytes', flush=True)
    assets = {entry['file']: entry for entry in corpus.assets.values()}
    (args.output/'manifest.json').write_text(json.dumps({
        'captured': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'pages': pages,
        'assets': assets, 'asset_failures': corpus.failures, 'skipped_pages': skipped}, indent=2)+'\n')


if __name__ == '__main__':
    main()
