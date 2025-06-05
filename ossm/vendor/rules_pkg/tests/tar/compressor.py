'''Fake compressor that just prepends garbage bytes.'''

import sys

GARBAGE = b'garbage'

if __name__ == '__main__':
    assert sys.argv[1:] == ['-a', '-b', '-c']
    sys.stdout.buffer.write(GARBAGE)
    sys.stdout.buffer.write(sys.stdin.buffer.read())
