#!/usr/bin/env python3

import sys
import math
import subprocess
import time
import os
import random

STATUSES = ['PASS', 'FAIL', 'TIMEOUT']
TURN_RED = '\033[58' '5;91m'
RESET_TEXT = '\033[58' '5;0m'
NUM_TRIES = 3
MOVE_TO_START_OF_ROW = '\033[' '1G'
NUM_TRIES_LEN = math.floor(math.log10(NUM_TRIES)) + 1

if len(sys.argv) != 2:
    sys.exit(f'Usage:\n{sys.argv[0]} exe_listing_file')

with open(sys.argv[1]) as f:
    def parse_line(line):
        split_line = [x.strip() for x in line.split('\0')]
        return {
            'name': split_line[0],
            'command': split_line[1:]
        }

    commands = [parse_line(x) for x in f.readlines()]

max_name_len = max([len(x['name']) for x in commands])
num_len = 1 + math.floor(math.log10(len(commands)))
status_len = max([len(x) for x in STATUSES])

error_happened = False
err_log = []
# For more randomness, and to try and allow consecutive runs to work, distribute the ports from
# 1500-65000 in groups of 10
ports = [*range(15005, 65000, 10)]
random.shuffle(ports)
port_index = 0

for i, command in enumerate(commands):
    start_time = time.monotonic()
    for attempt_no in range(NUM_TRIES):
        print(f'Running "{command["name"]}"; {attempt_no + 1:>{NUM_TRIES_LEN}}/{NUM_TRIES:>{NUM_TRIES_LEN}}', end='', flush=True)
        try:
            completed = subprocess.run(
                command['command'],
                timeout=15,
                capture_output=True,
                text=True,
                env={'START_PORT': str(ports[port_index]), **os.environ}
            )
            port_index += 1
            return_code = completed.returncode
            status_str = 'PASS' if return_code == 0 else 'FAIL'
            if return_code == 0:
                break
        except subprocess.TimeoutExpired:
            return_code = 1
            status_str = 'TIMEOUT'
            # Wait a tiny bit
            time.sleep(1)
            break
        finally:
            print(MOVE_TO_START_OF_ROW, end='')
    run_time = time.monotonic() - start_time
    to_print = '   '.join([
        f'{i + 1:>{num_len}}/{len(commands):>{num_len}}',
        f'{commands[i]["name"]:<{max_name_len}}',
        f'{status_str:<{status_len}}',
        f'{run_time:>5.2f}s'
    ])
    if return_code == 0:
        print(to_print)
    else:
        error_happened = True
        err_log.append({
            'name': command['name'],
            'command': ' '.join(command['command']),
            'stdout': completed.stdout,
            'stderr': completed.stderr,
            'returncode': completed.returncode,
            'timed_out': status_str == 'TIMEOUT'
        })
        print(f'{TURN_RED}{to_print}{RESET_TEXT}')

if len(err_log) > 0:
    for err in err_log:
        print(f'Test "{err["name"]}"')
        print(f'  Command {err["command"]}')
        if err['timed_out']:
            print(f'Timed out')
        else:
            print(f'Exited with status {err["returncode"]}')
        if err['stdout']:
            print('-' * 79)
            print(f'stdout:\n{err["stdout"]}')
        if err['stderr']:
            print('-' * 79)
            print(f'stderr:\n{err["stderr"]}')
        print('-' * 79)

sys.exit(int(error_happened))
