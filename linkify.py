import fileinput
import re
import sys

# linkify all cpp files that can be wrapped in ``, but don't linkify if they
# are wrapped in []. Also, don't linkify anything directly following a :
r = re.compile(r'(?<!\[|`)`?\b(\w+\.(?:cpp|c|h))\b`?')
ignore = re.compile(r'^\[[^\]]+\]:')

def linkify(line):
  if ignore.match(line):
    return line
  newline = r.sub(r'[`\1`](\1)', line)
  return newline

assert linkify('bla') == 'bla'
assert linkify('bla.h') == '[`bla.h`](bla.h)'
assert linkify('bla.html') == 'bla.html'
assert linkify('bla.c') == '[`bla.c`](bla.c)'
assert linkify('bla.cpp') == '[`bla.cpp`](bla.cpp)'
assert linkify('`bla.cpp`') == '[`bla.cpp`](bla.cpp)'
assert linkify('[`bla.cpp`]') == '[`bla.cpp`]'
assert linkify('[bla.cpp]') == '[bla.cpp]'  # not sure about this one
assert linkify('a.cpp and b.cpp') == '[`a.cpp`](a.cpp) and [`b.cpp`](b.cpp)'
assert linkify('[tut05]: tut05_parse.cpp') == '[tut05]: tut05_parse.cpp'




for line in fileinput.input():
   sys.stdout.write(linkify(line))
