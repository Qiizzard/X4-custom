"""
PlatformIO post-build script: fail when firmware.bin does not fit in the
smallest app partition from partitions.csv, and warn well before that.

The hard failure only fires at 100% full, which is too late to be useful when a
merge is steadily adding apps -- by then the work is done and the only options
are to undo it or repartition. So this also enforces a *headroom reserve*: drop
below it and the build says so loudly, while there is still room to change
course. See docs/merge/PORT_LEDGER.md for the arithmetic driving the default.

  custom_flash_reserve_bytes   platformio.ini option (bytes). 0 disables.
  CROSSINK_FLASH_FAIL_UNDER_RESERVE=1
                               make the reserve a hard failure (for CI once the
                               partition question is settled), not a warning.
"""

import csv
import os
import re
import sys


def _parse_size(value):
    value = value.strip()
    match = re.fullmatch(r'(\d+)([KkMm])?', value)
    if match:
        size = int(match.group(1), 10)
        suffix = match.group(2)
        if suffix in ('K', 'k'):
            return size * 1024
        if suffix in ('M', 'm'):
            return size * 1024 * 1024
        return size
    return int(value, 0)


def _get_project_option(env, name):
    try:
        value = env.GetProjectOption(name)
    except Exception:
        return None
    if isinstance(value, str):
        value = value.strip()
    return value or None


def _get_partition_file(project_dir, env):
    partition_file = _get_project_option(env, 'board_build.partitions')
    if not partition_file:
        return None
    if os.path.isabs(partition_file):
        return partition_file
    return os.path.join(project_dir, partition_file)


def _get_app_partition_limit(partition_file):
    app_sizes = []
    app_labels = []

    with open(partition_file, newline='', encoding='utf-8') as fp:
        for row in csv.reader(fp):
            if not row:
                continue

            row = [cell.strip() for cell in row]
            if not row[0] or row[0].startswith('#') or len(row) < 5:
                continue

            name, partition_type, _subtype, _offset, size = row[:5]
            if partition_type != 'app':
                continue

            app_labels.append(name)
            app_sizes.append(_parse_size(size))

    if not app_sizes:
        return None, None

    return min(app_sizes), ', '.join(app_labels)


def check_firmware_size(source, target, env):
    del source

    firmware_path = str(target[0])
    firmware_size = os.path.getsize(firmware_path)
    project_dir = env['PROJECT_DIR']
    partition_file = _get_partition_file(project_dir, env)

    if not partition_file or not os.path.exists(partition_file):
        print('Unable to find partition table for firmware size check', file=sys.stderr)
        env.Exit(1)

    limit, labels = _get_app_partition_limit(partition_file)
    if not limit:
        print(f'No app partition found in {partition_file}', file=sys.stderr)
        env.Exit(1)

    remaining = limit - firmware_size
    rel_partition_file = os.path.relpath(partition_file, project_dir)
    if remaining < 0:
        print(
            f'Firmware image is too large for OTA app partition: '
            f'{firmware_size} bytes > {limit} bytes '
            f'({rel_partition_file}: {labels}); over by {-remaining} bytes',
            file=sys.stderr,
        )
        env.Exit(1)

    used_percent = (firmware_size / limit) * 100
    print(
        f'Firmware image fits OTA app partition: '
        f'{firmware_size} bytes <= {limit} bytes '
        f'({remaining} bytes free, {used_percent:.1f}% used)',
    )

    reserve_option = _get_project_option(env, 'custom_flash_reserve_bytes')
    reserve = _parse_size(reserve_option) if reserve_option else 0
    if reserve and remaining < reserve:
        message = (
            f'FLASH HEADROOM LOW: {remaining} bytes free, below the '
            f'{reserve}-byte reserve ({rel_partition_file}: {labels}). '
            f'Every further app competes for what is left -- see '
            f'docs/merge/PORT_LEDGER.md before adding more.'
        )
        if os.environ.get('CROSSINK_FLASH_FAIL_UNDER_RESERVE') == '1':
            print(message, file=sys.stderr)
            env.Exit(1)
        print(f'WARNING: {message}')


try:
    Import('env')                                           # noqa: F821  # type: ignore[name-defined]
    env.AddPostAction(                                      # noqa: F821  # type: ignore[name-defined]
        '$BUILD_DIR/${PROGNAME}.bin',
        check_firmware_size,
    )
except NameError:
    print('check_firmware_size.py: must be run via PlatformIO', file=sys.stderr)
