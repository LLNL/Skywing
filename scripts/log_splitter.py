import re
import sys

if len(sys.argv) != 2:
  sys.exit(f'Usage: {sys.argv[0]} log_file')

log_re = re.compile('\[([^\]]*)\] \[([^\]]*)\] \[([^\]]*)\] "([^"]*)"(.*)')

logs = {}

with open(sys.argv[1]) as f:
  for row in f:
    try:
      time, level, source, id_, message = log_re.match(row).groups()
      if id_ not in logs:
        logs[id_] = []
      logs[id_].append((time, level, source, f'"{id_}"{message}'))
    except AttributeError:
      pass

for id_, lines in sorted(logs.items()):
  print(f'{id_}:')
  for time, _, _, line in lines:
    print(f'\t[{time}] {line}')
