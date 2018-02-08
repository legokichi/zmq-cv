import argparse
from datetime import datetime as dt
from datetime import timedelta, timezone
import sys
import pytz
import re
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='')
    parser.add_argument("--input",  action='store', type=str)

    args = parser.parse_args()
    with open(args.input, "r") as f:
        line = f.readline()
        time_str = line.split("\t")[0] # "2017-10-17 08:04:10.486000"
        if re.search(r"\d{6}$", time_str) != None:
            sys.stdout.write(pytz.timezone('UTC').localize(dt.strptime(time_str, '%Y-%m-%d %H:%M:%S.%f')).astimezone(pytz.timezone('Asia/Tokyo')).strftime('%Y-%m-%d_%H:%M:%S%z'))
        else:
            sys.stdout.write(pytz.timezone('UTC').localize(dt.strptime(time_str, '%Y-%m-%d %H:%M:%S')).astimezone(pytz.timezone('Asia/Tokyo')).strftime('%Y-%m-%d_%H:%M:%S%z'))