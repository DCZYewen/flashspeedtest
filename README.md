# flashspeedtest

A Linux CLI tool for benchmarking read/write throughput of block devices and mounted filesystems. Designed for testing USB drives, SD cards, SSDs, HDDs, and other storage media.

## Features

- Filesystem mode — safe read/write test via a temporary file
- Raw block device mode — direct O\_DIRECT I/O, bypasses filesystem (destructive)
- Data integrity verification (CRC32 per block)
- Bypasses page cache via O\_DIRECT for accurate disk-level measurements
- Single static binary, no runtime dependencies

## Build

```sh
make
```

Clean build artifacts:

```sh
make clean
```

## Cross-Compilation

Install a cross-compiler toolchain, then set `CC` and `LDFLAGS`:

```sh
# aarch64 (ARM 64-bit)
make CC=aarch64-linux-gnu-gcc LDFLAGS="-static"

# arm (ARM 32-bit)
make CC=arm-linux-gnueabihf-gcc LDFLAGS="-static"

# RISC-V 64
make CC=riscv64-linux-gnu-gcc LDFLAGS="-static"
```

The `-static` flag produces a self-contained binary with no shared library dependencies — ideal for flashing onto embedded targets.

## Usage

```
flashspeedtest [OPTIONS] <target>
```

### Filesystem mode (safe)

Test read/write speed through a mounted filesystem. Creates a temporary test file, benchmarks, then cleans up.

```sh
flashspeedtest /mnt/usbdrv
flashspeedtest -bs 4K -sz 64M /mnt/sdcard
flashspeedtest -w /mnt/ssd           # write only
flashspeedtest -r /mnt/ssd           # read only
flashspeedtest -verify /mnt/ssd      # verify data integrity
```

No root required if you have write permission to the target directory.

### Raw block device mode (destructive!)

Test raw device speed directly, bypassing the filesystem. **All data on the device will be destroyed.**

```sh
sudo flashspeedtest -raw /dev/sdc
sudo flashspeedtest -raw -bs 4K -sz 16M /dev/sdc
```

Safety guards:
- Requires root privilege
- Double confirmation (yes + type device name)
- Refuses mounted devices
- Refuses system disks (`/dev/sda`, root device)

### Options

| Flag       | Argument | Default | Description                                    |
|------------|----------|---------|------------------------------------------------|
| `-raw`     | —        | off     | Raw block device mode (destructive)            |
| `-bs`      | size     | `1M`    | Block size per I/O op (e.g. `4K`, `64K`, `1M`) |
| `-sz`      | size     | `256M`  | Total data size to test                        |
| `-w`       | —        | off     | Write test only                                |
| `-r`       | —        | off     | Read test only                                 |
| `-rw`      | —        | on      | Read + write test (default)                    |
| `-verify`  | —        | off     | Verify data integrity after write              |
| `-count`   | n        | `1`     | Number of test iterations                      |
| `-progress`| —        | off     | Show live progress bar (adds I/O overhead)     |
| `-y`       | —        | off     | Skip confirmations (for scripted use)          |
| `-h`       | —        | —       | Show help                                      |

Size suffixes: `K` (KiB), `M` (MiB), `G` (GiB).

## Run Tests

```sh
make test            # runs as current user (raw tests skipped)
sudo make test       # runs all tests including raw block device
```

## Example Output

```
flashspeedtest v1.0.1
Target:     /mnt/usbdrv (ext4, /dev/sdb1)
Block size: 1M
Total size: 256M
Mode:       Sequential Read+Write

Writing... 100%  274.3 MB/s
  Write:   274.3 MB/s (256 MB in 0.93s)
Reading... 100%  1295.7 MB/s
  Read:   1295.7 MB/s (256 MB in 0.20s)
  Verify: 256/256 blocks OK
```

## Exit Codes

| Code | Meaning                        |
|------|--------------------------------|
| 0    | Success                        |
| 1    | Usage error or safety refusal  |
| 2    | I/O failure during benchmark   |

## Project Structure

```
├── Makefile
├── include/          # headers
│   ├── bench.h
│   ├── io.h
│   ├── output.h
│   ├── parse.h
│   └── safety.h
├── src/              # implementations
│   ├── main.c
│   ├── bench.c
│   ├── io.c
│   ├── output.c
│   ├── parse.c
│   └── safety.c
├── test.sh           # integration test suite
└── plan.md           # design document
```
