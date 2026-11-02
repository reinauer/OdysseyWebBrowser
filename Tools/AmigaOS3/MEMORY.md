# Browser memory and performance

Recorded 2026-10-30. These figures are for running the browser. Unpacking the
archive is separate. All results are for OWB
`74b6ade751d6e79d4b81299dbd6632e8b112a95b0c17325f434a5645b4c0ce94`.

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
not been determined.

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
