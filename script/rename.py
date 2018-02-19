import sys
from datetime import datetime, timezone, timedelta
JST = timezone(timedelta(hours=+9), 'JST')
for line in sys.stdin:
    if len(line) > 0:
        sys.stdout.write(str((datetime.strptime(line.strip(), '%Y-%m-%d %H:%M:%S')+ timedelta(hours=+9)).replace(tzinfo=JST)))