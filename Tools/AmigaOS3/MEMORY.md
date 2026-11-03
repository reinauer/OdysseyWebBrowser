# Browser memory and performance

Recorded 2026-10-30. These figures are for running the browser. Unpacking the
archive is separate. All results are for OWB
`74b6ade751d6e79d4b81299dbd6632e8b112a95b0c17325f434a5645b4c0ce94`, the
software floating point build. The FPU builds made on 2026-11-02 need less;
see [Results of the changes](#results-of-the-changes).

## Answer

**Use 256 MiB. The lowest level which passed was 143 MiB free at launch, so
allow at least 150 MiB free.** A 128 MiB machine starts and browses light pages,
but it froze on a heavy page.

| Question | Answer |
| --- | --- |
| Can OWB load at all? | Only with a single free block of about 89 MiB. |
| Used at the first page request | 97 MiB. |
| Used after a minimal page | 104 MiB. |
| Peak on light pages | 111 MiB. |
| Peak on the heaviest page (GitHub) | 130 to 136 MiB, depending on free memory. |
| Lowest level which passed | 143.0 MiB free, with 13.2 MiB left at worst. |
| Highest level which failed | 127.0 MiB free. |

These levels were measured with Zune68's MUI5 build. Genuine MUI 5.0 peaked at
142.3 MiB with 393.6 MiB free, in one visit to all six pages, about 7 MiB more
than Zune68 at 511.6 MiB free. Its lowest working level has not been measured,
so do not apply the 143 MiB figure to it.

The levels between 127 and 143 MiB free were not tested, because no recommended
configuration would depend on them. A real installation will use more than the
8.4 MiB this minimal test system needed, and its TCP/IP stack is additional:
WinUAE's socket emulation uses no guest memory for one. Measure free memory on
the actual system, with its stack running.

## Measurements

Each run visited all six corpus pages twice, except where noted. "Installed"
means a real memory configuration. The others held memory back with
`MemoryProbe --leave`, which is the closest way to test a size no board offers.

| Memory | Free at launch | Result | Peak used | Lowest free | Smallest largest block |
| --- | --- | --- | --- | --- | --- |
| 512 MiB plus 8 MiB installed | 511.6 MiB | Pass | 135.5 MiB | 376.1 MiB | 359.4 MiB |
| 256 MiB installed | 247.6 MiB | Pass | 130.0 MiB | 117.6 MiB | 103.4 MiB |
| Held to 192 MiB, four visits | 191.0 MiB | Pass | 132.1 MiB | 58.9 MiB | 38.8 MiB |
| Held to 160 MiB | 159.0 MiB | Pass | 130.2 MiB | 28.8 MiB | 8.5 MiB |
| Held to 144 MiB | 143.0 MiB | Pass | 129.8 MiB | 13.2 MiB | 8.0 MiB |
| Held to 128 MiB | 127.0 MiB | Froze, second Wikipedia visit | 126.2 MiB | 0.8 MiB | 0.0 MiB |
| 128 MiB installed | 119.6 MiB | Froze, second Wikipedia visit | 118.8 MiB | 0.8 MiB | 0.2 MiB |
| Held to 110 MiB | 109.0 MiB | Froze, first Wikipedia visit | 108.7 MiB | 0.3 MiB | 0.0 MiB |

The 256 and 128 MiB runs have Zorro III memory only. The held runs start from
the 512 MiB Zorro III and 8 MiB Zorro II configuration.

Peak use falls with free memory because `ramSize()` reads free memory after OWB
has loaded, and that value selects smaller caches. Results therefore cannot be
extrapolated from a larger run; each size has to be measured.

Every failing run had also used chip memory, 1.0 to 1.3 MiB of it, because Exec
falls back to chip memory when fast memory runs out. No passing run used any.

The archive itself was then tested: `ce-packaged-256` ran from the tree native
LhA unpacked from `Odyssey-AmigaOS3.lha`, without `Resource/icudt49b`. One visit
to all six pages passed on 256 MiB installed, with a 128.6 MiB peak.

### How it fails

The browser gives no warning. There is no requester, error or exit, and the
status bar shows `Ready`. Depending on where it stopped, the page stays unstyled
or the old page remains with grey holes. Memory then stays unchanged and the
system keeps running, so OS3 itself survives. The reason the page load stops has
not been determined. (This describes the builds of that time. "Running out of
memory" below found the cause, and the browser now reports it; see "Fatal
errors".)

Total free memory does not explain the freeze on its own. The installed 128 MiB
run stopped with 2.4 MiB free, the 127.0 MiB run with 9.5 MiB free and the
109.0 MiB run with 2.6 MiB free, but their largest blocks were only 0.7, 1.6 and
0.65 MiB. An 800x600x32 backing store needs about 1.9 MiB in one piece, so
fragmentation is the likely limit, though this is an inference. Allow headroom
beyond the peak for that reason.

### Longer sessions

Four visits at 191.0 MiB free passed. Use after each Wikipedia visit rose from
118.3 to 119.8, 120.8 and 121.7 MiB, so memory grows by about 1 MiB per round.
Six pages visited four times does not establish long-session behaviour, and
sessions of hours or days have not been tested.

## Performance

Reference CPU: cycle-exact 68030 at about 50 MHz, which calibrated to 12.5 MIPS
on a fixed instruction loop. Three million software double additions and
multiplications took 81 seconds. Times are guest seconds to the load event at
511.6 MiB free. Launch to the first page request took 26 to 28 seconds.

| Page | Elements | First visit | Second visit |
| --- | --- | --- | --- |
| text.npr.org | 80 | 2.5 s | 11.1 s |
| Hacker News | 822 | 21.0 s | 28.0 s |
| Aminet | 2,623 | 52.2 s | 60.5 s |
| amiga-news.de | 800 | 11.3 s | 11.2 s |
| Wikipedia: Amiga | 5,772 | 150.8 s | 133.4 s |
| GitHub repository page | 1,687 | 298.3 s | 276.9 s |

Pages are served from loopback, so these times include no Internet latency.
Real connections, and TLS in particular, will be slower.

The slow second text.npr.org visit follows GitHub, and repeated at about 11
seconds in every passing run. With 119.6 MiB free it took 3.3 seconds. The cause
has not been investigated.

## Stack

Recorded 2026-11-01. **The browser uses a 2 MiB main stack.** Its own stack is
allocated at startup unless it was launched with at least that much, because
newlib's startup ignores `__stack`. A 4 KiB launch stack crashed with
`#80000004` in "Initializing Fonts" on 68030, 68040 and 68060 before this.

| Page | Deepest main stack use |
| --- | --- |
| Startup to the first page | 65,392 bytes |
| All six corpus pages, including Wikipedia and GitHub | 70,464 bytes |
| Divs written 600 deep | 207,168 bytes |
| Nested lists, 500 open tags | 237,004 bytes |
| Flexbox written 600 deep | 377,988 bytes |
| Floats written 600 deep | 1,623,896 bytes |
| Floats built 3,000 deep by script | 1,625,776 bytes |

These used an 8 MiB launch stack. `MemoryProbe.c` scans for the lowest nonzero
word, so a page may have used slightly more. The test pages are in
`build/amigaos3-stack-corpus` and are not committed.

The float figures are the worst case found. Depth is bounded because WebCore
stops creating renderers at `Settings::defaultMaximumRenderTreeDepth`, 512, which
is the likely reason 3,000 levels use about the same stack as 600. 2 MiB leaves
about 460 KiB beyond that. A smaller stack would suit every page except nested floats, but layout
recursion has no stack check and an overflow silently corrupts memory, so 2 MiB
costs about 1.5% of peak browser memory to rule that out. Worker threads have
512 KiB each, and three were seen.

WTF records the launch stack during static construction, before `main`. After
moving the stack `main` must call `wtfAmigaOS3StackSwapped`. Without it, each
collection scanned from the new stack to the old origin: about 14 MiB, and the
first report took 388.8 seconds under GDB rather than 15.2. With it, launches
from 1 MiB and 4 KiB took 15.3 and 15.2 seconds.

Genuine MUI 5.0 launched from 4 KiB (`re-corpus-mui5`, 68040) passed one visit
to all six pages with a 70,632 byte peak.

## Executable size

| Section | Bytes | Loaded? |
| --- | --- | --- |
| Code hunk | 88,773,228 | Yes, as one contiguous allocation |
| Data hunk | 98,588 | Yes |
| BSS hunk | 223,716 | Yes |
| Relocations | 5,609,340 | No |
| Symbols | 7,955,356 | No |

The code hunk sets the hardware floor. Total RAM is not enough: two 64 MiB
boards cannot load OWB, because neither offers the block it needs. Fragmented
memory can also prevent loading on a system which reports enough free.

The 68EC020 in a stock A1200 has a 24-bit address bus and cannot reach this much
memory, so in practice this means an accelerator with 32-bit addressing.

## Method

- `MemoryProbe.c --sample` records Exec free memory each second from a task
  above normal priority, reopening its log so that a crash loses no samples.
- `MemoryProbe.c --leave` holds memory before launch, simulating a smaller
  system. It holds the largest blocks first. The result is 8 MiB of Zorro II
  fast memory plus the rest of Zorro III, not a real board layout.
- `fetch-memory-corpus.py` snapshots public pages with their CSS, scripts and
  images into `build/amigaos3-memory-corpus`. Snapshots are third-party content
  and are not committed. This corpus was captured on 2026-10-30.
- `run-browser-test.py --memory-probe --corpus` browses the snapshots and writes
  each page's timing and memory to `result.json`.

Each page stays open for five seconds after its load event. Results are in
`build/amigaos3-memory`: `ce-512`, `ce-real-256`, `ce-real-128`,
`ce-leave-192-long` and `ce-leave-*`.

Do not use `cpu_speed=max` for timing. It calibrated to only 5.4 MIPS on this
host, slower than the cycle-exact 50 MHz 68030. Its guest clock also ran at about
half of host time. All earlier fixture timings used this mode and do not
represent a hardware speed.

One `cpu_speed=max` run took 62 seconds to load text.npr.org, about ten times
what its CPU speed predicts. It was sharing the host with other emulators and
this has not been reproduced or explained.

## Limits of these results

- WinUAE only. No physical Amiga has been tested.
- One window and one tab. More of either will use more memory.
- `OWB_DISABLE_TOPSITES_SCREENSHOTS` is set, as in the tested configuration.
- Audio and video are disabled in this build.
- The 16 MiB of RTG memory is separate from these figures.
- Snapshots keep their static content, but requests made later by page scripts
  are not recorded. Live pages may use more memory.
- The minimal test system has no real TCP/IP stack, Workbench tools or
  commodities, all of which reduce the memory left for the browser.

## Findings for reducing memory and time

Recorded 2026-11-02 for OWB
`057dd6cbc5e709470e006f5e93f78b5bf6142143c71fa880ae8149647dff5187`. These are
findings and estimates, not results of changed builds. Savings are estimates
unless the text says they were measured.

### What fills the code hunk

Symbol sizes from `m68k-amigaos-nm -n`, measured as the distance to the next
symbol:

| Part | MiB | Share |
| --- | --- | --- |
| WebCore | 36.9 | 43.6% |
| ICU data, one object `_icudt62_dat` | 25.6 | 30.2% |
| JavaScriptCore | 8.5 | 10.0% |
| WTF | 3.5 | 4.2% |
| Other C++ and C | 5.4 | 6.3% |
| libxml/xslt, sqlite, libstdc++, cairo, freetype, curl, pixman, images | 4.7 | 5.6% |

WebCore's generated JS bindings are 9.9 MiB of its total; SVG is about 4.8
MiB, Inspector code about 1.6 MiB and IndexedDB about 1.3 MiB. The remaining
optional features (MathML, XSLT, CSS Typed OM, paint worklets, notifications,
fullscreen, MHTML, plugins) are each under 0.5 MiB, so disabling features is
not a useful lever.

### ICU data

The package `icudt62b.dat` is 25.5 MiB, linked into the code hunk because
genccode's constant array lands in `.text`.

| Part | MiB |
| --- | --- |
| Converters, 190 files | 4.89 |
| Break iteration, including the 2.0 MiB Chinese/Japanese dictionary | 3.34 |
| Collation | 3.22 |
| Locale data | 2.88 |
| Time zone names | 2.48 |
| Currency names | 2.40 |
| Language names | 1.95 |
| Unit names | 1.46 |
| Region names | 0.98 |
| Transliteration | 0.66 |
| Spellout rules | 0.52 |
| Other (normalization, properties, character names, stringprep) | 0.71 |

WebKit decodes Chinese, Japanese, Korean and single-byte encodings with its
own codecs (TextCodecCJK, TextCodecSingleByte). `TextCodecICU.cpp` opens only
19 ICU converters, 0.58 MiB including the 0.52 MiB EUC-TW table; ISO-8859-16
is not in this ICU data at all. Dropping the other converters saves 4.3 MiB
and no WebKit encoding. Keeping 21 languages (en, de, fr, it, es, pt, nl,
sv, da, nb, no, fi, pl, cs, hu, el, ru, tr, ja, zh, ko) with their regional
variants, the needed converters, and dropping the Chinese/Japanese word
dictionary, transliteration and character names leaves 8.3 MiB, saving 17.2
MiB. Intl formatting for other languages then falls back to root data, and
word selection in Chinese and Japanese text loses its dictionary.

Moving the data out of `.text` into its own hunk would reduce the largest
single block needed to load OWB by its size, without saving memory.

### Compiler options

Release builds use `-O3`. Sixteen sampled unified sources (twelve WebCore,
four JavaScriptCore) were recompiled with other options:

| Option | Code and data | Relative |
| --- | --- | --- |
| `-O3` | 1,028,268 bytes | 100% |
| `-O2` | 858,396 bytes | 83.5% |
| `-Os` | 533,300 bytes | 51.9% |

Applied to the 52 MiB of compiled C++, `-O2` would save about 8.6 MiB and
`-Os` about 25 MiB. The speed of either has not been measured. There are no
frame pointers in the current code.

The shipped OWB is not stripped: 7.9 MB of its 102.4 MB file are symbols,
which LoadSeg reads and discards.

### Runtime heap

- `ramSize()` returns `AvailMem(MEMF_CHIP)+AvailMem(MEMF_FAST)` once, after OWB
  has loaded (`Source/WTF/wtf/RAMSize.cpp`). `OWB_RAM_SIZE` (MiB) overrides it.
- The memory cache is 8 MiB below a `ramSize()` of 512 MiB, and the
  back/forward cache keeps one page from 256 MiB (`WebView.cpp`, about 5 MiB
  by comparing the 512 and 256 MiB runs).
- JavaScriptCore asks `tryFastAlignedMalloc(16 KiB, 16 KiB)` for each
  MarkedBlock. The OS3 implementation over-allocates `size+alignment+3`
  bytes, 32,771 for 16 KiB, which takes the SDK allocator's large-block path.
  Every GC block costs twice its size, and per-type blocks are never
  released because decommit does nothing on OS3.
- The JavaScript interpreter stack reserves 5 MiB (`maxPerThreadStackUsage`).
- The SDK allocator (MemMap) adds a 16-byte header and rounds to 16 bytes;
  `malloc(8)` takes 32 bytes. Requests of 32,748 bytes and more each get an
  `AllocMem(MEMF_ANY)`, which can fall into chip memory, and small-block pages
  are scattered between them in Exec's first-fit list. `realloc` always copies.
- WTF threads each get a 512 KiB stack.
- SQLite connections have no `cache_size`, so each can grow to about 2 MiB.
- Decoded frames of an animated image are kept up to 30 MiB per image.
- `deadDecodedDataDeletionInterval` is 0 for the document browser model, so
  decoded data of unused resources is not released on a timer.
- Each hidden tab keeps its 1.8 MiB cairo surface at 800x600; both
  `screenshot()` overloads leak a surface; the palette path allocates a full
  conversion buffer on every update.
- Nothing responds to low memory. The generic MemoryPressureHandler does
  nothing, the `setLowMemoryHandler` calls are under `#if 0`, and F12's cache
  pruning therefore has no effect. WebCore's own allocations abort on failure,
  while cairo, pixman and curl return NULL quietly, which may explain the silent
  stop. An Exec `AddMemHandler` could release a reserve block, signal the main
  task, and run WebCore's `releaseMemory()`.

### Profile

A PC sampler over one GDB remote connection (5 samples per second) ran during
the corpus on the cycle-exact 68030: 2,120 samples over four pages and part of
Wikipedia. Guest page times matched the unsampled runs (2.48 s, 20.9 s, 51.9 s
and 11.2 s). The runner stopped during Wikipedia because sampling slowed the
host. Launch and GitHub are not included.

| Busy time | Share |
| --- | --- |
| Software floating point | 27.3% |
| FreeType, mostly the TrueType bytecode interpreter `TT_RunIns` | 9.4% |
| malloc and free, including Exec semaphores | 8.9% |
| Other OWB code: style 6.5%, layout 5.2%, curl 3.3%, cairo 2.7% of all samples | 54.4% |

16.3% of all samples were outside OWB with no OWB return address on the
stack: idle time or other tasks. The JavaScript interpreter was under 1% on
these pages. Samples are attributed to the preceding symbol, so static
functions can carry a neighbour's name, and library time is attributed to the
first OWB return address found on the stack.

### CPU findings

- Double arithmetic comes from GCC's 68000 `lb1sf68.S`. `__muldf3` multiplies
  one bit per pass, 53 passes; the 27 µs per operation above follows from this.
  It never uses an FPU. Single-precision arithmetic and conversions call the
  ROM math libraries, which use an FPU when present.
- The `-m68020` code contains about 4,600 32x32->64 multiplies, which the
  68060 does not implement and 68060.library emulates; `-m68020-60` avoids
  them.
- `amigaos3MonotonicTime` opens and closes timer.device on every call and
  converts through three software double operations. `gettimeofday` opens
  locale.library on every call; sleeping creates a message port and opens
  timer.device each time.
- `Thread::current()` uses `pthread_getspecific`, which searches the thread
  list and takes an Exec semaphore. The `tc_UserData` shortcut is only enabled
  for OS4, MorphOS and AROS.
- Atomics are library calls guarded by `Forbid`/`Permit`.
- The main loop wakes on a 14 ms tick, no RunLoop wake-up is installed, and
  curl sleeps 10 ms per pass.
- About 18 s of the 27 s launch pass after the hunks are allocated with no
  further allocation, which points to loading and relocating the 102 MB file
  (about 1.4 million relocations). This is inferred from the memory log.
- The fontconfig cache is in `T:`, so it is rebuilt after every reboot.

### Decisions

Recorded with the user on 2026-11-02:

- Build for 68020-68060 with an FPU (`-m68020-60 -m68881`); drop the
  software floating point build.
- Use `-O2`. Measure `-Os` for speed before deciding on it.
- Trim the ICU data, give it its own hunk, and strip the shipped executable.
- Keep the back/forward cache at 256 MiB and above.
- Add low-memory handling, fix the aligned GC block allocation, replace the
  allocator, shrink the interpreter stack and fix the smaller items above.
- Open timer.device once, and use `tc_UserData` for `Thread::current()` if
  the pthread library allows it.

## Results of the changes

Recorded 2026-11-02. Every item under Decisions is implemented. The builds
are `build/amigaos3-browser-fpu` (`-O2`, OWB `57f46e57`) and
`build/amigaos3-browser-fpu-os` (`-Os`, OWB `6f4234c9`). Both use
`-m68020-60 -m68881` and the reduced ICU data. They were measured like the
runs above: cycle-exact 68030, 512 MiB Zorro III plus 8 MiB Zorro II, two
visits to the corpus. The earlier build, OWB `74b6ade7` (`-O3`, software
floating point, full ICU data), is the reference.

### Size

| Build | File | Stripped | Code hunk | Data hunk |
| --- | --- | --- | --- | --- |
| Earlier, `-O3` (OWB `057dd6cb`) | 102,444,124 | 94,488,384 | 88,779,580 | 98,588 |
| `-O2` | 75,195,644 | 66,910,560 | 52,610,416 | 9,439,712 |
| `-Os` | 57,704,232 | 48,372,940 | 34,793,988 | 9,439,712 |

The data hunk now holds the 9.0 MiB of ICU data, so the largest block
needed to load OWB is the code hunk: 50.2 MiB at `-O2` and 33.2 MiB at `-Os`,
down from 84.7 MiB. The staged executable is stripped; the build tree keeps
the symbols.

### Speed

Guest seconds to the load event:

| Page | Earlier | `-O2` | `-Os` | `-Os` against `-O2` |
| --- | --- | --- | --- | --- |
| text.npr.org | 2.5 | 2.4 | 2.6 | +5% |
| Hacker News | 21.0 | 17.6 | 22.2 | +26% |
| Aminet | 52.2 | 42.2 | 56.4 | +34% |
| amiga-news.de | 11.3 | 9.6 | 12.7 | +33% |
| Wikipedia: Amiga | 150.8 | 109.0 | 144.4 | +32% |
| GitHub repository page | 298.3 | 235.0 | 336.7 | +43% |
| Second visits, in order | 11.1, 28.0, 60.5, 11.2, 133.4, 276.9 | 7.2, 20.1, 34.2, 9.4, 93.0, 226.4 | 9.6, 26.1, 53.1, 12.7, 128.7, 316.1 | +29% to +55% |

`-O2` takes 15 to 43% less time than the earlier build on every page but the
smallest. `-Os` gives all of that up: overall it is as slow as the earlier
software floating point build, and up to 13% slower on GitHub and
amiga-news.de. Launch to the first page request took 24.6 seconds at `-O2`
and 23.4 at `-Os`, against 27.5; the smaller file loads faster.

WinUAE's cycle-exact 68030 does not charge the execution time of FPU
instructions, so these runs understate the time a real 68881 or 68882 takes.
The gain over software floating point will be smaller on hardware than shown
here. This affects `-O2` and `-Os` alike, since both execute the same
floating point work.

### Memory

| | Earlier | `-O2` | `-Os` |
| --- | --- | --- | --- |
| Used at the first page request | 97.2 MiB | 69.5 MiB | 52.5 MiB |
| Peak, two visits | 135.5 MiB | 106.4 MiB | 89.9 MiB |
| Used at the end | 133.6 MiB | 100.7 MiB | 83.5 MiB |

`-O2` saves 28 to 33 MiB. `-Os` saves another 17 MiB throughout, which is the
difference in code size.

Held to 128 MiB, the run that froze on the second Wikipedia visit now passes
both visits. It peaked at 102.1 MiB with at least 24.9 MiB free, the largest
free block never fell below 15.3 MiB, and no chip memory was used. The
earlier build reached 0.0 MiB in its largest block and used 1.3 MiB of chip
memory. Memory never ran out in this run; it did in the runs below.

### Checks

- JavaScriptCore: 2,142 checks with no failures at `-O2` and at `-Os`
  (`build/amigaos3-jsc-fpu`, `build/amigaos3-jsc-fpu-os`). The tagged-pointer,
  precise-allocation (11,095 checks) and math tests also pass at `-O2`.
- The browser fixture passes at `-O2` (`build/amigaos3-browser-fpu-fixture`).
- Without an FPU, OWB prints that it needs one and exits with 20.
- On a 68040 (A4000 layout, Kickstart 3.2 A4000 ROM), `-O2` visited all six
  corpus pages with the Workbench 3.2 SetPatch 47.4, which loaded
  68040.library 46.3 and mmu.library 46.21. It peaked at 104.2 MiB, in
  `fast-fpu-a4000-wb32`, with the fixture copy
  `build/amigaos3-amissl-fixture-wb32cpu`. Without SetPatch
  (`fast-fpu-a4000-nosp`) it peaked at 104.4 MiB. The page times of the two
  runs differ by at most 1%, apart from the 1.2-second first page, which
  was 0.1 s quicker with SetPatch. The shared
  fixture's older SetPatch 44.44 and 68040.library 37.30 jumped into unmapped
  memory or reset the guest before OWB started.
- On a 68060 (same layout and SetPatch, which loaded 68060.library 46.4), the
  browser page checks passed (`fpu-a4000-060-wb32`), peaking at 78.8 MiB. The
  emulator log shows the FPU disabled at boot and enabled during the
  Startup-Sequence, which is the library's work.
- On a 68LC040 with the same SetPatch and 68040.library, OWB printed that it
  needs an FPU and exited with 20 (`lc040-wb32`). A small program without that
  check stopped at its first FPU instruction with an FPU disabled exception
  and printed nothing (`lc040-mathtest`). The Workbench 3.2 CPU libraries
  (68040.library 46.1) emulate the FPU instructions that the 68040 and 68060
  lack, but not a missing FPU. SoftIEEE (Aminet util/libs, 40.7.1) does: it
  gives FPU-less CPUs from the 68000 to the 68LC060 a virtual FPU, and ships
  an updated 680x0.library that loads it; MMULib's 680x0.library does the
  same from release 47.9.1. Unless its HIDDEN option is set, SoftIEEE
  announces the FPU in `AttnFlags`, which is what OWB's startup check reads,
  so the FPU build should start. That has not been tried here. Every floating
  point instruction then traps into the emulation, so expect it to be far
  slower than the software floating point build. Without such an emulation,
  a 68020 or 68030 without a 68881/68882, a 68LC040, a 68LC060 or a 68EC060
  cannot run the FPU build.

WinUAE's default FPU compares by subtraction, so equal infinities compared
unequal and one JavaScriptCore check failed; it also ignores the FPCR
rounding precision. The runners now set `fpu_strict=true`, which fixes both
on the host FPU. The measurements above ran with WinUAE's softfloat 68882,
which is also exact but slower on the host.

### Running out of memory

| Held to | Result |
| --- | --- |
| 128 MiB, two visits | Passed, at least 24.9 MiB free |
| 96 MiB, one visit | Passed Wikipedia, stopped on GitHub |
| 80 MiB, one visit | Stopped on Aminet |

The 96 and 80 MiB runs stopped like the earlier build: no message, the previous page still
shown and `Receiving data` in the status bar. GDB showed the same cause in
both, which is likely also the cause of the earlier freezes. The stacks and
screenshots are `gdb-stacks.txt` and `gdb-frozen.png` in `ce-fpu-leave-96`
and `ce-fpu-leave-80`.

1. `WTF::fastMalloc` failed, at 80 MiB in inline layout and at 96 MiB while
   decoding a GitHub style sheet. WebKit treats a failed allocation as fatal,
   and `oomCrash` calls `abort()`.
2. The SDK's `abort()` writes `Program aborted` to standard error, which did
   not reach the test's output file, and calls `_exit()`, which runs the
   exit functions.
3. The SDK's pthread exit function joins every thread. The curl scheduler
   thread was still polling and two WTF threads were waiting, so the join
   never returns. The handlers that stop curl and the work queues at Quit
   had not run. The main task is blocked, so nothing redraws.

The low-memory handler did not prevent either stop. Its 1 MiB reserve can
let one failing allocation succeed, but caches are only released from the
main loop, and the main task did not get back there before the next failure.

Two changes would address this. The first is now implemented:

- Make the fatal path visible and finite (below).
- Release caches before allocation fails: check free memory and the largest
  block from the main loop, and release at a threshold. Not done.

### Fatal errors

The link now routes `abort()` to `__wrap_abort` in
`Source/WTF/wtf/amigaos3/Fatal.c` (`--wrap=abort`); 13,786 calls in the
executable go there. The wrapper frees the low-memory reserve and a second
1 MiB block that only a fatal error frees, prints `OWB: fatal error: ...` to
standard error, calls the browser's handler (`fatal_error` in
`Tools/OdysseyWebBrowser/main.cpp`), and if that returns, leaves through
`_exit(20)` as the SDK's `abort()` leaves through `_exit(127)`. The handler:

- The first error shows a requester with one button, Quit: "Odyssey ran out
  of memory and has to quit." (`oomCrash` sets that reason), or for a failed
  check "Odyssey stopped at an internal error and has to quit." with the
  code offset of the call, to look up with nm in the same build.
- On the main task inside the main loop, Quit leaves the loop through
  `bailout_env`, releases caches as after a low-memory signal, and shuts down
  as for a normal Quit; OWB returns 20.
- On another thread, it sets a flag that makes the main loop quit without
  the safe-to-quit check, signals Ctrl-C, and ends the thread with
  `pthread_exit()`, since the exit code joins every thread. Work queues keep
  a reference to their run loop for this case.
- Anywhere else, including a second error during that shutdown, it stops the
  timer, curl, localStorage and work queue threads without allocating
  (`StorageThread::killAllForExit` kills the queue; queued writes are lost)
  and leaves through `_exit(20)`.

The runner now sets `FailAt 21` before starting OWB, so a fatal exit is
recorded as `RETURN=20` instead of stopping the Startup-Sequence.

Tests on the 68040 with 80 MiB left and the default corpus (`fpu-fatal-80`
to `fpu-fatal4-80`) all failed the same allocation in inline layout on the
third page, and showed the requester (`requester.png`). With 80 MiB left the
heap was only 8 to 9 MB at that point: the executable's hunks take 59.4 MiB.
Quit was pressed with Left-Amiga B through the emulator's input queue.

- Run 1: the shutdown ran out of memory again in `WebView::~WebView`, and a
  first-entry guard in the wrapper (`Wait(0)` for a second call) stopped the
  main task. The guard is gone.
- Run 2: the second error reached the handler and stopped curl, but the exit
  code then waited for the localStorage thread. The handler now stops it and
  the timer thread too, and the reserve for fatal errors was added.
- Run 3: the shutdown failed again; the handler stopped the threads and OWB
  exited (with 127, the SDK `abort()`'s code; now 20).
- Run 4: the shutdown completed; `RETURN=20`. A breakpoint on the wrapper saw
  no second call.

When the second error happens while MUI disposes the application, the
application object stays behind with its ARexx port; in runs 1 to 3 the
browser window was already closed by then. Releasing caches before
allocation fails would make that path rarer.

### Thread stacks

WTF gives every thread a 512 KiB stack. Three threads ran during the 68040
corpus visit. After GitHub, the lowest non-zero word in their stacks was 416,
6,200 and 7,016 bytes below the top, and everything below that was still
zero, so the stacks started out cleared and these figures are close to the
real use. A frame that reserves space without writing it could reach
further. The stack size is unchanged: 64 KiB would save about 1.3 MiB, but other
pages may start other threads or recurse deeper.

### GitHub page

GitHub takes 121 to 124 s on the 68040 (A4000 layout, Workbench 3.2
SetPatch). The snapshot loads 2.4 MiB of CSS in 19 files, 1.3 MiB of
JavaScript in 9 files (one of 890 KiB) and has 128 inline SVGs and 1,687
elements.

The snapshot tool rewrites only stylesheet links. The page's other remote
`<link>` tags stay: preloaded images, icons, `preconnect` and
`dns-prefetch`. Its 112 `modulepreload` tags are ignored, because this WebKit
does not support them. During the page the emulator opened five HTTPS
connections to GitHub's CDN, and the curl thread ran AmiSSL's bignum code for
6.5% of the samples. With those 131 tags removed
(`fast-fpu-a4000-ghlocal`), the emulator made no outside connections and the
page loaded in 109.3 s instead of 123.6 s (DOMContentLoaded 60.8 s instead of
75.9 s). The corpus figures for GitHub therefore include about 14 s of TLS
and live network. `fetch-memory-corpus.py` now drops non-stylesheet `<link>`
tags that point to another host; the existing snapshot predates that change.

A PC sampler that also recorded the running task (`fast-fpu-a4000-ghprof2`,
3,751 samples during the page) attributed the main task's samples as follows:

| Main task | Share |
| --- | --- |
| CSS: tokenizer 12%, selector matching 13%, rule parsing and cascade 7%, other 4% | 36% |
| JavaScript parsing and bytecode generation; running JavaScript was 0.3% | 12% |
| WTF strings: CSS input preprocessing, hashing, atoms | 10% |
| malloc and free | 9% |
| ROM (graphics, Exec, Intuition) | 7% |
| Other WebCore | 7% |
| Layout 4%, FreeType 3%, everything else | 19% |

On each 10 ms pass the curl thread reads the clock through `clock_gettime`,
which calls timer.device's `ReadEClock` (CIA reads) and did three 64-bit
divisions in libgcc. With an FPU it now does one floating point division
instead. No sample landed in a system math library. OWB does not
open one: the FPU build uses FPU instructions and the FPU `libm`. MUI 5 and its
classes, Zune, AmiSSL and texteditor.gadget open the IEEE double libraries.

Follow-up measurements with the build that has the FPU clock and no vtable
check (`fpu-ghcount`, `fpu-ghnet-h6`, `fpu-ghnet-h2`, `fpu-ghreuse`):

- The clock change did not change the page time: 109.1 s locally (was
  109.3) and 123.8 s with the live links (was 123.6).
- Limiting curl to two connections per host
  (`WEBKIT_CURL_MAX_HOST_CONNECTIONS 2`) gave 124.1 s against 123.8 s with
  the default of six, so it saves nothing here.
- Breakpoints on `CSSTokenizer`'s constructor counted 396 tokenizer runs
  over 3.77 million characters; the page's style sheets hold 2.55 MB. GitHub
  links one 304,457-byte sheet (`@layer primer-react`) five times, from
  separate server-rendered React parts. It was downloaded once but tokenized
  and parsed five times. Every other sheet was tokenized once.
- `HTMLLinkElement::setCSSStyleSheet` ran 23 times. `restoreParsedStyleSheet`
  never found a saved sheet, and `saveParsedStyleSheet` ran for only 16 of
  the 23. Reading `StyleSheetContents` in `isCacheable()` (`fpu-ghcache`)
  showed that the shared sheet loaded completely and without error, but its
  `m_hasSyntacticallyValidCSSHeader` was false: the parser found its first
  rule invalid. Cascade layers are an experimental feature in this WebKit
  and off, so `consumeLayerRule` returns nothing and the whole
  `@layer primer-react{...}` block is dropped. The same happens to an
  822,640-byte `@layer primer-brand{...}` sheet and an 11 KB sheet that
  starts with an `@layer a,b,c;` statement: 1.14 MB of GitHub's 2.54 MB of
  style sheets is tokenized and then thrown away, and those styles never
  apply.
- `CSSParserImpl::consumeRuleList` now counts a first `@layer` rule as a
  valid header while layers are off; `isCacheable()` reads the flag and
  nothing else does. In `fpu-ghcss` the 304 KB sheet was tokenized once and
  restored for the other four links, all 19 parsed sheets were saved,
  tokenized text fell from 3.77 to 2.57 million characters, and the page
  loaded in 104.1 s instead of 109.2 (DOM ready 55.4 s instead of 60.7).
  Peak memory did not change (100.7 MB): the repeated parses only made
  short-lived tokens. That run shared the host with another emulator; alone
  and without the probe (`fpu-ghcss2`) the page loaded in 103.4 s, against
  109.0 to 109.2 s in the three earlier runs on the same snapshot.
- Turning cascade layers on would render GitHub's Primer styles, but this
  WebKit has no CSSOM wrapper for layer rules (`createCSSOMWrapper` has
  `case StyleRuleType::Layer: // FIXME: Implement.`), so a script reading
  `cssRules` of such a sheet would get a null rule. It would also make style
  matching do more work. Not done.
- `Document::resolveStyle` ran 31 times during the page and the style
  resolver was rebuilt 5 times.

The sampler's stop requests are not served at random guest times. input.device
showed up in 0 to 33% of the samples, depending on the window, always at the
same instructions of Intuition's 10 Hz timer-event handler. A breakpoint
there was hit ten times per guest second. Shares of tasks woken by timer
interrupts are inflated; time spent within OWB's own busy code is not
affected. The earlier 68030 profile above has the same bias in its "outside
OWB" figure.

### Gmail sign-in

With the vtable check off (see the last fault below), Google's sign-in page
loads on the 68040 (A4000 layout, WinUAE at full host speed) and no longer
stops. It is still unusable: every key press runs Google's `keydown`
listener for about 20 s. Over one minute, OWB's main loop completed a pass
every 22.9 s, and ten stack snapshots all showed `JSEventListener::handleEvent`
under `WebViewPrivate::onKeyDown`. Free memory stayed at 274 MiB, and three
connections to Google stayed open.

| Main task during the listener (400 samples) | Share |
| --- | --- |
| LLInt C loop | 43% |
| Scope lookups: `hasProperty`, `JSLexicalEnvironment::getOwnPropertySlot`, `JSScope::resolve`, slow paths | about 20% |
| `__atomic_compare_exchange_1` and libatomic's lock | about 10% |
| GC marking (`JSLexicalEnvironment::visitChildren`) | 6% |

The atomic calls come from `JSLexicalEnvironment::getOwnPropertySlot`, which
locks the symbol table: `ConcurrentJSLock` was always a `WTF::Lock`, whose
fast path is a 1-byte compare-and-swap. The toolchain never inlines `cas`,
even for `-m68040`; libatomic wraps a plain compare in `Forbid()` and
`Permit()`. Read-modify-write bus cycles are not safe on chip RAM, so inline
`cas` is not an option. `ConcurrentJSLock` is now a `NoLock` on AmigaOS 3,
and the options force concurrent GC off and one GC marker. Without a JIT,
the collector thread runs only while the world is stopped, so no second
thread visits symbol tables, structures or code blocks.

A temporary counter in the scope slow paths (`gsignin-scope`, fresh fixture
without cookies, page load plus typing) showed why the lookups were slow.
Of 2,550,000 slow-path calls, 1,251,504 `resolve_scope` and 1,246,881
`get_from_scope` calls had the type `ClosureVarWithVarInjectionChecks`: closure
variables that JSC had resolved statically. They take the slow path because
the global object's var injection watchpoint had fired. A sloppy-mode direct
`eval` that declares a `var` in any function scope fires it, and from then on
every closure variable read below a function that uses `eval` walks the
whole scope chain with a symbol table lookup per scope. The hot code had no
source URL, so it was itself evaluated code.

An injected `var` becomes a property of the activation it lands in, which
then leaves the plain activation structure. The 32-bit LLInt now checks the
scopes between a read and its variable for that structure when the
watchpoint has fired, and reads the variable directly if none has changed.
Otherwise `slow_path_resolve_scope` checks only whether one of those scopes
has a property of this name before it falls back to `JSScope::resolve`. In
the `jsc` shell, a loop reading four closure variables took 5.7 s, and
24.7 s once an eval had injected a var anywhere; with the change it took
6.0 s. Ten tests of the eval cases in `jsc-scope-test.js` (shadowing,
injection after the closure was used, `delete`, writes, deep chains, block,
catch and `with` scopes) and the 2142 checks of `jsc-test.js` pass. The page corpus
loads with the same element counts (`fpu-corpus-nolock`,
`fpu-corpus-scopefix`). The time per key press on Google's page has not been
measured again.

### Faults found on the way

- CMake resolved pkg-config's `-lm` to `m68k-amigaos/lib/libm.a`, the 68000
  software floating point library, which returns doubles in d0/d1. `pow()`
  then returned garbage to FPU code and OWB aborted at startup.
  `AmigaOS3.cmake` now searches the multilib directory of the CPU flags.
- Closing the shared timer request from a C++ destructor ran before the SDK
  joined the remaining threads at exit. A thread still sleeping then used the
  closed request and crashed with `#80000003`. The request is now closed by
  an exit function after the join, and a sleep after that opens its own.
- Google's sign-in page froze OWB: the page was drawn but took no input. The
  generated JavaScript bindings check that a wrapped object's vtable pointer
  equals the address of the class's vtable symbol (`BINDING_INTEGRITY`). The
  executable holds two identical copies of `AbortSignal`'s vtable, at
  `0x318a740` and `0x31fbaa0`; the object used one and the binding the other.
  The failed `RELEASE_ASSERT` called `abort()`, and the exit then waited
  forever for the other threads, as in "Running out of memory". The check is
  now off for AmigaOS 3, as it already was for Windows.
