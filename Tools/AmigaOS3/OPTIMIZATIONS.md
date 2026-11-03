# Speed-up candidates

Recorded 2026-11-02 and 2026-11-03, starting from commit `08ee4c57` (FPU
build, `-O2`). This lists speed-ups not tried before, where the evidence for
each came from, and the results of trying them. Profiles and the changes
made before are in [MEMORY.md](MEMORY.md). Unless a figure says it was
measured, it is an estimate from those profiles.

## Where the time goes

| Profile | Largest shares |
| --- | --- |
| Page corpus, cycle-exact 68030, before the FPU build | Software floating point 27.3% (now gone), FreeType 9.4% (mostly the TrueType bytecode interpreter `TT_RunIns`), malloc and free 8.9% |
| GitHub snapshot, 68040, main task | CSS 36% (tokenizer 12%, selector matching 13%), JavaScript parsing and bytecode generation 12% (running it 0.3%), WTF strings 10%, malloc and free 9%, ROM 7%, layout 4%, FreeType 3% |
| Google sign-in key press, 68040 | LLInt C loop 43%, scope lookups about 20% and atomics about 10% (both since fixed), GC marking 6% |

Launch takes 16.4 s on the 68040 and 24.6 s on the 68030 to the first page
request. On the 68030 about 18 s of the 27 s launch of the `-O3` build passed
with no allocation after the hunks were loaded, which points to loading and
relocating the executable. That is inferred from the memory log only.

## Already ruled out

- The C loop interpreter already uses computed goto
  (`ENABLE_COMPUTED_GOTO_OPCODES`).
- Two connections per host instead of six made no difference on GitHub.
- `-Os` saves 17 MiB but is 26 to 43% slower than `-O2` on most pages.
- Converting the curl clock with the FPU did not change page times.

## Candidates that reduce function slightly

1. **Font hinting off.** OWB follows fontconfig's pattern, whose default
   turns full hinting on, so FreeType runs each glyph's TrueType
   instructions. One `<match>` in `fonts.conf` turns it off. Glyphs become
   unhinted outlines, slightly softer at small sizes.
2. **Animation limits.** Force `prefers-reduced-motion` (WebCore setting
   `ForcedPrefersReducedMotionAccessibilityValue`), cap the rendering update
   rate, and play animated images once. The rate is already 30 frames per
   second on AmigaOS (`AnimationFrameRate.h`) and halves in low-power mode.
   OWB stores an "Enable animation" preference, but nothing in WebCore
   reads it, and its looping preference is commented out in
   `appclass.cpp`. The corpus pages are mostly
   static; the expected gain is on pages with spinners, carousels or
   `requestAnimationFrame` loops. Animations become choppier.
3. **JavaScript or images off per site.** OWB's URL settings already do
   this. On GitHub, parsing JavaScript was 12% of the main task and running it
   0.3%; the page is rendered by the server. No code change.
4. **Web fonts off.** Icon fonts turn into boxes. GitHub and Wikipedia use
   few web fonts, so the corpus gain would be small. Not planned.

## Candidates without loss of function

5. **TLS reuse.** curl was built `--without-nghttp2`, so each host got up to
   six HTTP/1.1 connections and each needed a TLS handshake. On GitHub five
   live connections cost about 14 s of AmiSSL bignum work. The local corpus
   cannot show this; live browsing has never been timed. Session sharing
   through the share handle is not needed: curl 8.23 keeps a TLS session
   cache in the multi handle (`multi->ssl_scache`), which all requests use
   because the share handle does not share `CURL_LOCK_DATA_SSL_SESSION`. On
   MUI the scheduler keeps that multi handle until the browser quits
   (`stopThreadIfNoMoreJobRunning()` is compiled out), so connections and
   sessions survive between pages. Whether AmiSSL resumed sessions was
   unknown; `DEBUG_CURL` only logs in debug builds.
6. **Persistent font cache.** `fonts.conf` puts the cache in `T:fontconfig`,
   so it is rebuilt on the first launch after every reboot. The splash
   window's `FcInit()` and `FcFontList()` build it.
7. **Lazy image loading.** WebKit has it, but it is experimental and off,
   and OWB does not enable it. 21 of the 25 images in the corpus Wikipedia
   page are `loading="lazy"`.
8. **`-O3` for the interpreter alone.** `LowLevelInterpreter.cpp` holds the C
   loop, 43% of Google's key press.

## Larger projects

- A native m68k backend for JSC's offline assembler instead of the C loop.
- Less CSS work: the tokenizer copies each sheet while preprocessing it, and
  GitHub's 2.5 MB of style sheets dominate its page time.
- Faster launch: confirm first where the 16 to 25 seconds go.

## Results

All corpus runs: 68040 (`cpu_model=68040`, A4000 chipset, `cpu_speed=max`),
`build/amigaos3-amissl-fixture-wb32cpu`, Zune68 Beta 2 libraries and
classes, OWB on the DOS7 data hardfile, corpus `build/amigaos3-memory-corpus`
(one cycle, 5 s settle). Times are `loadEventEnd`. Four earlier runs of the
unchanged browser in this setup differ by at most 0.4 s per page, so that is
the noise level. The baseline is `fpu-corpus-zune1102`.

| Page | Baseline | Hinting off (1) | Warm font cache (6) |
| --- | ---: | ---: | ---: |
| text-news | 1.2 s | 1.1 s | 1.2 s |
| link-list | 8.9 s | 6.1 s | 8.9 s |
| aminet | 19.1 s | 17.6 s | 19.6 s |
| amiga-news | 7.0 s | 6.4 s | 7.0 s |
| wikipedia | 53.3 s | 47.3 s | 53.3 s |
| github | 115.0 s | 113.7 s | 116.2 s |
| Sum | 204.5 s | 192.2 s | 206.2 s |
| Launch to first request | 16.4 s | 16.4 s | 15.2 s |

**1. Hinting off: adopted.** 6% less over the corpus and up to 31% on
text-heavy pages (link-list), with no change in memory. One `<match>` in
`Tools/AmigaOS3/dependencies/fonts.conf` assigns `hinting` false. At
GitHub's 16 px menu text the two screenshots differ only in edge pixels
(867 of 324,220 pixels); no difference is visible.

**6. Persistent font cache: not worth it.** A probe (`fcwarm`) timed
`FcInitLoadConfigAndFonts()` plus `FcFontList()` before OWB started:

| Configuration | Empty cache | Cache written |
| --- | ---: | ---: |
| OWB's `fonts.conf` (one font in `PROGDIR:Fonts`) | 0.02 s | 0.02 s |
| 20 TrueType files | 0.19 s | 0.02 s |

Rebuilding the cache after a reboot costs about 10 ms per font file. The
1.2 s shorter launch in the warm run is one memory-probe sample (the probe
samples once per second); the samples before it match those of the baseline.

**7. Lazy image loading: small gain, not adopted.** With
`setLazyImageLoadingEnabled(true)` in `WebView.cpp` (run `fpu-opt-lazy`) the
other pages are unchanged and Wikipedia makes 6 requests instead of 27:

| Wikipedia | Baseline | Lazy images |
| --- | ---: | ---: |
| `loadEventEnd` | 53.3 s | 49.4 s |
| `DOMContentLoaded` | 16.6 s | 48.1 s |
| Peak memory used | 105 MiB | 104 MiB |

The 3.9 s saved is about the cost of fetching and decoding the 21 offscreen
images. DOMContentLoaded comes 31 s later: the layout and painting that
followed parsing in the baseline now seem to happen before parsing ends,
probably because the lazy-image intersection observer needs layout while the
page is still being parsed. That was not traced. Scripts that wait for
DOMContentLoaded run that much later, and images below the fold load only
when scrolled to, which on this CPU means waiting while scrolling.

**8. `-O3` for `LowLevelInterpreter.cpp`: 2% on JavaScript, optional.** The
C loop's object grows from 129 KB to 146 KB of code. `jsc` with only that
file at `-O3` passes `jsc-test.js` (2142 checks). A benchmark of nine small
workloads (property access, closures, calls, arrays, strings, regular
expressions, JSON, `Math`, allocation; best of three rounds each) ran in
`run-platform-test.py` (68020 with 68882, `cpu_speed=max`), `-O2` before and
after `-O3`:

| Build | Total | properties | calls | Others |
| --- | ---: | ---: | ---: | ---: |
| `-O2` | 13067 ms | 957 ms | 2424 ms | 9686 ms |
| `-O3` | 12802 ms | 871 ms | 2312 ms | 9619 ms |
| `-O2` again | 13069 ms | 957 ms | 2425 ms | 9687 ms |

The two `-O2` runs differ by 2 ms, so the 2.0% is real. The workloads that
stay in the interpreter loop gain 5 to 9%; the others spend their time in
runtime functions compiled at `-O2`. The corpus pages run little
JavaScript, so their times would not change measurably. A script-heavy page
like Google's key press (43% in the C loop) would gain roughly 2 to 4%.

**5. TLS reuse: sessions already resume; HTTP/2 saves 28% on a live page,
adopted.**
The local HTTPS test of `run-browser-test.py`, with the server closing each
connection and logging Python's `session_reused` (run `tls-reuse`), shows
AmiSSL resuming TLS 1.3 sessions from curl's multi-handle cache:

| Connection | Handshake (host time) |
| --- | ---: |
| `localhost`, first | 4.05 s |
| `localhost`, second, resumed | 2.39 s |
| `127.0.0.1` (other name, so no session) | 3.11 s |

TLS 1.3 resumption still does the key exchange, so it saves about 40%, not
all, of a handshake. It only helps connections opened after the first
handshake to a host has finished; the parallel connections of one page all
start at once and do full handshakes.

For HTTP/2, nghttp2 1.70.0 (static, `--enable-lib-only`, same CPU flags) and
curl 8.23.0 `--with-nghttp2` were built in the scratchpad and OWB was
relinked with them; `CurlContext` already asks for HTTP/2 over TLS with
`CURLOPT_PIPEWAIT` when curl supports it. OWB grows by 180 KB. The test page
loads 19 public Wikimedia thumbnails (18 from `thumb.wikimedia.org`, one from
`upload.wikimedia.org`, about 270 KB); Wikimedia sends `Timing-Allow-Origin`,
so Resource Timing reports protocol and connection setup per image. Runs
alternated:

| Run | Protocol | New connections | Sum of TLS setup | DOMContentLoaded | Load |
| --- | --- | ---: | ---: | ---: | ---: |
| `live-h1a` | HTTP/1.1 | 5 | 21.1 s | 24.3 s | 29.1 s |
| `live-h2a` | HTTP/2 | 2 | 13.0 s | 16.6 s | 21.0 s |
| `live-h1b` | HTTP/1.1 | 6 | 21.9 s | 24.3 s | 30.1 s |
| `live-h2b` | HTTP/2 | 2 | 12.8 s | 16.6 s | 21.0 s |

Peak memory was the same (82 to 83 MiB). DOMContentLoaded of this tiny page
moves with the handshakes: their bignum work runs on the network task and
takes CPU time from the main task's parsing.

Adopted: `build-dependencies.py` builds nghttp2, `build-curl.py` configures
`--with-nghttp2`, and CMake links `libnghttp2.a` after curl. The rebuilt
OWB `99eae0e2`, which also has hinting off, passed the curl probe
(`build/amigaos3-curl-fpu/network-check-h2`), the HTTPS, WebSocket and
navigation fixture (`fpu-h2-fixture`) and the live page again in 21.0 s
(`live-h2c`). On the corpus (`fpu-h2-corpus`) the six pages took 186.3 s;
GitHub, whose snapshot still loads images and scripts from GitHub's servers,
took 108.8 s against 113.7 s with hinting off alone:

| Page | Baseline | Hinting off | Hinting off and HTTP/2 |
| --- | ---: | ---: | ---: |
| text-news | 1.2 s | 1.1 s | 1.2 s |
| link-list | 8.9 s | 6.1 s | 6.0 s |
| aminet | 19.1 s | 17.6 s | 17.3 s |
| amiga-news | 7.0 s | 6.4 s | 6.4 s |
| wikipedia | 53.3 s | 47.3 s | 46.6 s |
| github | 115.0 s | 113.7 s | 108.8 s |
| Sum | 204.5 s | 192.2 s | 186.3 s |

**2. Animation limits: CSS animations cost most.** A synthetic two-page
corpus (`animcorpus` in the scratchpad) runs the same JavaScript work loop
for 15 s, yielding with `postMessage` so that timer alignment cannot affect
it, and reports work units per second and `requestAnimationFrame` frames.
The static page has only text. The animated page adds four rotating
border-radius spinners and an opacity/width pulse (CSS animations, disabled
by a `prefers-reduced-motion: reduce` rule as many sites do), three 64x64
animated GIFs at 20 frames per second, and a `requestAnimationFrame` loop
that updates a text node. A variant OWB reads `OWB_ANIMLIMIT`: `r` forces
`prefers-reduced-motion`, `l` sets low-power mode, which halves the
rendering update rate to 15 per second.

| Setting | Static page | Animated page | Frames in 15 s |
| --- | ---: | ---: | ---: |
| None (two runs) | 228, 227 | 13, 13 | 9, 9 |
| `r` reduced motion | 226 | 148 | 110 |
| `l` low power | 226 | 13 | 9 |
| `rl` both | 226 | 148 | 110 |

Unlimited, the animations take 94% of the main task: one frame took about
1.6 s, so halving the frame rate cap changes nothing. Stopping the CSS
animations brings back most of the throughput; the GIFs and the
`requestAnimationFrame` loop still cost 35%.

Forcing `prefers-reduced-motion` only helps on sites that ask for it, so a
second variant stops animation itself, again under `OWB_ANIMLIMIT`:

- `c`: CSS animations get zero delay and zero iteration duration in
  `CSSAnimation::syncPropertiesWithBackingAnimation()`, so they finish at
  once (on their last keyframe with `forwards` fill, otherwise on the
  element's own style, as spinners usually are), and `Styleable` starts no
  transitions, so style changes apply at once.
- `g`: `BitmapImage::shouldAnimate()` returns false, so animated GIF, APNG
  and WebP images stay on their first frame.

| Setting | Animated page | Frames in 15 s |
| --- | ---: | ---: |
| None | 13 | 9 |
| `g` | 13 | 9 |
| `c` | 148 | 111 |
| `gc` | 176 | 132 |

Freezing images alone does nothing while CSS animations force a repaint
every frame; with both off, only the `requestAnimationFrame` loop's 22%
remains. On the corpus (`fpu-opt-animoff`, `gc`) GitHub loads in 106.1 s
instead of 115.0 s (8% less), the other pages are unchanged, the element
counts match and GitHub's final screen is identical to the baseline. The
corpus pages are not interacted with, so a missing `transitionend` event
(zero-duration transitions fire none) would not have shown; scripts that
wait for one could hang a menu or dialog. Giving transitions 1 ms instead
of none would keep the events.

Not adopted yet: the switch should be a WebCore setting driven by OWB's
existing "Enable animation" preference, which is stored today but reaches
nothing in WebCore.
