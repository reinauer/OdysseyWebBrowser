# Native SQLite VFS

SQLite 3.46.1 is built with `SQLITE_OS_OTHER`, a real `amigaos3` VFS and
pthread mutexes. The source patch makes the mutex selector honor the
explicit pthread choice instead of also enabling its no-op mutex backend.
The default VFS uses AmigaDOS handles directly, not the SDK's partial POSIX
file implementation. The browser selects DELETE rollback journals on OS3;
it does not select WAL or disable journaling.

Supported operations include checked reads/writes, zero-filled short reads
and sparse writes, truncation, metadata, file/directory flushing, paths,
access checks, temporary files and record locking. Synchronization uses
`Flush` for file handles and `ACTION_FLUSH` on the directory's filesystem
handler after journal deletion. The VFS does not claim atomic sectors or
other optimistic device capabilities. AmiSSL supplies random bytes through
the existing task-local Odyssey entropy helper.

SQLite's PENDING, RESERVED and SHARED lock ranges are implemented using
DOS record locks. Lock ownership is per DOS file handle. An atomic sibling
directory named `<database>.sqlite-create-lock` serializes CREATE opens
among VFS users, including exclusive creation. An existing guard returns
BUSY immediately. The VFS never guesses whether another process's guard is
stale: after an interrupted create, recovery requires checking that no
owner is active and manually removing the guard. Programs writing the
database outside this VFS do not participate in this advisory protocol.

Initial limits: signed 31-bit file offsets (files below 2 GiB), rollback
journals only, no mmap/shared memory/WAL, no loadable extensions, and memory
temporary storage by default. This is not proof of physical-drive
power-failure behavior. These limits are enforced by the build and VFS,
rather than advertising unsupported features.

## Reproduce the disk test

```sh
python3 Tools/AmigaOS3/build-dependencies.py \
  --sqlite-smoke --sqlite-volume=DH1: sqlite
xdftool build/amigaos3-deps-other/sqlite-data.hdf \
  create chs=1024,1,32 + format SQLiteData DOS3
```

Run `Tools/AmigaOS3/run-platform-test.py` with the existing AmiSSL fixture,
the generated `sqlite-smoke` binary, `--data-hdf` pointing to that image,
and success marker `PASS native SQLite smoke`. The runner copies the image
and mounts it as DH1 with 32 sectors, one head, two reserved blocks and
512-byte blocks. The original blank image is preserved.

Native FFS validation passed in `build/amigaos3-sqlite-ffs-1/output.txt`:

* Commit, file flush, journal deletion and reopen retain committed values.
* A separate DOS process fails to acquire the reserved writer lock with
  SQLITE_BUSY while the first transaction owns it.
* Rollback followed by close/reopen restores the previous value.
* Exclusive creation rejects an existing file; an existing create guard
  returns BUSY and remains intact.

The final database was extracted from the post-run FFS image with xdftool.
Independent host SQLite verification returned `ok` from `integrity_check`
and the final committed value 92. Native RAM filesystem tests also passed
in `build/amigaos3-sqlite-ram-1`; this is separate evidence from the FFS run.

The WinUAE directory filesystem used for SYS cannot validate this locking
protocol in the tested emulator: `filesys.cpp`'s `action_free_record`
(around lines 4529-4541) never advances its `prev` list pointer. Unlocking
a non-head record discards newer record locks, causing a subsequent unlock
to fail with ERROR_RECORD_NOT_LOCKED. The VFS protocol was not weakened to
accommodate that emulator defect; native guest RAM and FFS handlers were
used instead. No WinUAE source was modified.
