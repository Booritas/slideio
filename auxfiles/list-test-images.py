#!/usr/bin/env python3
"""Regenerate software-docs/TEST_IMAGES.md.

Parses every TestTools::getTestImagePath / getFullTestImagePath call in the test
sources, resolves the calls whose arguments are string literals against the three
data-root environment variables, stats what it finds, and writes the table.

Run from the repository root, with the environment the tests use:

    python3 auxfiles/list-test-images.py

Sizes and the present/absent split describe the machine it runs on, so the
committed document is a snapshot of whoever generated it last.
"""
import collections
import glob
import io
import os
import re

REPO = os.getcwd()
PARENT = os.path.dirname(os.path.abspath(REPO))
DOC = 'software-docs/TEST_IMAGES.md'

ROOT_VAR = {'full': 'SLIDEIO_IMAGES_PATH',
            'std': 'SLIDEIO_TEST_DATA_PATH',
            'priv': 'SLIDEIO_TEST_DATA_PRIV_PATH'}
ROOTS = {key: os.environ.get(var, '') for key, var in ROOT_VAR.items()}

BINARY = {'main': 'slideio_tests', 'ndpi': 'slideio_ndpi_tests', 'vsi': 'slideio_vsi_tests',
          'pke': 'slideio_pke_tests', 'ometiff': 'slideio_ometiff_tests',
          'phtiff': 'slideio_phtiff_tests', 'converter': 'slideio_converter_tests',
          'transformer': 'slideio_transformer_tests'}

CALL = re.compile(r'TestTools::get(Full)?TestImagePath\s*\(')
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
TEST_DECL = re.compile(r'^TEST(_F)?\(\s*(\w+)\s*,\s*(\w+)\s*\)', re.M)


def call_arguments(text, open_paren):
    """Text between the call's parentheses, by balancing them."""
    depth = 0
    for i in range(open_paren, len(text)):
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
            if depth == 0:
                return text[open_paren + 1:i]
    return None


def split_arguments(argument_text):
    """Top-level comma split that respects string literals and nesting."""
    parts, current, depth, in_string = [], '', 0, False
    i = 0
    while i < len(argument_text):
        char = argument_text[i]
        if in_string:
            current += char
            if char == '\\':
                current += argument_text[i + 1]
                i += 2
                continue
            if char == '"':
                in_string = False
        elif char == '"':
            in_string = True
            current += char
        elif char in '([':
            depth += 1
            current += char
        elif char in ')]':
            depth -= 1
            current += char
        elif char == ',' and depth == 0:
            parts.append(current)
            current = ''
        else:
            current += char
        i += 1
    if current.strip():
        parts.append(current)
    return [p.strip() for p in parts]


def as_literal(argument):
    """Concatenated string literals -> their value, else None.

    Tolerates u8/L/u/U/R prefixes. Only the escapes that occur in these paths are
    undone, so UTF-8 bytes survive (there are Cyrillic directory names in the corpus).
    """
    pieces = LITERAL.findall(argument)
    if not pieces:
        return None
    residue = LITERAL.sub('', argument)
    for prefix in ('u8', 'L', 'U', 'u', 'R'):
        residue = residue.replace(prefix, '')
    if residue.strip():
        return None
    return ''.join(p.replace('\\\\', '\\').replace('\\"', '"') for p in pieces)


def collect():
    references, dynamic = [], []
    sources = sorted(glob.glob('src/tests/*/*.cpp')) + sorted(glob.glob('src/single_tests/*/*.cpp'))
    # testtools.cpp only DEFINES the helpers; its parameter lists are not call sites.
    sources = [s for s in sources if not s.endswith('testlib/testtools.cpp')]
    for path in sources:
        text = io.open(path, encoding='utf-8', errors='surrogateescape').read()
        suite = path.split('/')[2]
        declarations = [(m.start(), m.group(2) + '.' + m.group(3)) for m in TEST_DECL.finditer(text)]

        def enclosing(position):
            name = '(outside a test)'
            for offset, test_name in declarations:
                if offset <= position:
                    name = test_name
                else:
                    break
            return name

        for match in CALL.finditer(text):
            argument_text = call_arguments(text, match.end() - 1)
            if argument_text is None:
                continue
            arguments = split_arguments(argument_text)
            subfolder = as_literal(arguments[0]) if arguments else None
            image = as_literal(arguments[1]) if len(arguments) > 1 else None
            private = len(arguments) > 2 and 'true' in arguments[2].lower()
            line = text[:match.start()].count('\n') + 1
            if subfolder is None or image is None:
                dynamic.append((path, line, enclosing(match.start()),
                                ' '.join(argument_text.split())[:80]))
                continue
            root = 'full' if match.group(1) else ('priv' if private else 'std')
            references.append(dict(root=root, subfolder=subfolder, image=image, suite=suite,
                                   test=enclosing(match.start())))
    return references, dynamic


def directory_size(path):
    total = 0
    for directory, _, files in os.walk(path):
        for name in files:
            try:
                total += os.path.getsize(os.path.join(directory, name))
            except OSError:
                pass
    return total


def relative(path):
    return os.path.relpath(path, PARENT) if path else ''


def human(size):
    if size is None:
        return '--'
    for unit, divisor in (('G', 1 << 30), ('M', 1 << 20), ('K', 1 << 10)):
        if size >= divisor:
            return '%.1f %sB' % (size / divisor, unit)
    return '%d B' % size


def main():
    references, dynamic = collect()

    images = {}
    for reference in references:
        root = ROOTS.get(reference['root'], '')
        absolute = (os.path.normpath(os.path.join(root, reference['subfolder'], reference['image']))
                    if root else None)
        key = (reference['root'], reference['subfolder'], reference['image'])
        entry = images.setdefault(key, dict(root=reference['root'], absolute=absolute,
                                            subfolder=reference['subfolder'],
                                            tests=set(), suites=set()))
        entry['tests'].add(reference['test'])
        entry['suites'].add(reference['suite'])

    for entry in images.values():
        path = entry['absolute']
        entry['exists'] = bool(path) and os.path.exists(path)
        entry['isdir'] = bool(path) and os.path.isdir(path)
        if not entry['exists']:
            entry['size'] = None
        elif entry['isdir']:
            entry['size'] = directory_size(path)
        else:
            entry['size'] = os.path.getsize(path)

    rows = sorted(images.values(), key=lambda e: (-(e['size'] or -1), relative(e['absolute'])))
    present = [e for e in rows if e['exists']]
    absent = [e for e in rows if not e['exists']]
    total = sum(e['size'] for e in present)

    out = []
    add = out.append
    add('# Test Images')
    add('')
    add('Every test image referenced from `src/tests/` and `src/single_tests/`, with where it')
    add('lives and what it costs on disk. Written to support deciding what to keep when the')
    add('corpus does not fit: the last column is what deleting an image would cost in tests.')
    add('')
    add('Paths are relative to the directory **containing** the `slideio` repository, so that')
    add('all three data roots can be written the same way.')
    add('')
    add('| Root | Environment variable | Path |')
    add('|---|---|---|')
    for key in ('full', 'std', 'priv'):
        add('| `%s` | `%s` | `%s` |' % (key, ROOT_VAR[key],
                                        relative(ROOTS[key]) if ROOTS[key] else '(unset)'))
    add('')
    add('| Suite | Test binary |')
    add('|---|---|')
    for suite, binary in sorted(BINARY.items()):
        add('| `%s` | `%s` |' % (suite, binary))
    add('| *(others)* | standalone programs under `src/single_tests/` |')
    add('')
    add('The **Tests** column counts the distinct `TEST`/`TEST_F` cases naming the image. The')
    add('programs under `src/single_tests/` have no test cases, so they show `--`.')
    add('')
    add('## Summary')
    add('')
    add('| | Count | Size |')
    add('|---|---:|---:|')
    add('| Images referenced by tests | %d | |' % len(rows))
    add('| Present on this machine | %d | **%s** |' % (len(present), human(total)))
    add('| Referenced but absent | %d | -- |' % len(absent))
    add('| Static references in test code | %d | |' % len(references))
    add('| References built at run time | %d | |' % len(dynamic))
    add('')

    # size present, images present, images absent -- keyed by root + subfolder argument
    rollup = collections.defaultdict(lambda: [0, 0, 0])
    for entry in rows:
        bucket = rollup['%s/%s' % (relative(ROOTS.get(entry['root'], '')), entry['subfolder'])]
        if entry['exists']:
            bucket[0] += entry['size']
            bucket[1] += 1
        else:
            bucket[2] += 1
    add('### By directory')
    add('')
    add('| Directory | Size present | Images present | Images absent |')
    add('|---|---:|---:|---:|')
    for key in sorted(rollup, key=lambda k: -rollup[k][0]):
        size, count, missing = rollup[key]
        add('| `%s` | %s | %d | %d |' % (key, human(size) if count else '--', count, missing))
    add('')

    def table(entries, title, note):
        add('## %s' % title)
        add('')
        add(note)
        add('')
        add('| Location | Size | Suite | Tests |')
        add('|---|---:|---|---:|')
        for entry in entries:
            named = len([t for t in entry['tests'] if t != '(outside a test)'])
            add('| `%s`%s | %s | %s | %s |' % (
                relative(entry['absolute']) if entry['absolute'] else '(root unset)',
                ' *(dir)*' if entry['isdir'] else '',
                human(entry['size']),
                ', '.join(sorted(entry['suites'])),
                named if named else '--'))
        add('')

    table(present, 'Images present, largest first',
          'Sizes are what is on disk now. "Tests" counts the distinct tests naming the image, '
          'which is what deleting it would cost.')
    table(absent, 'Images referenced but not present',
          'Referenced by the tests below but absent from this machine. These are the tests that '
          'fail, or that skip when `SLIDEIO_SKIP_MISSING_IMAGES` is set.')

    add('## References built at run time')
    add('')
    add('These call sites compose the path from a variable or a constant, so the file cannot be')
    add('identified by reading the source. They are not counted in the tables above.')
    add('')
    add('| File | Line | Test | Arguments |')
    add('|---|---:|---|---|')
    for path, line, test, arguments in sorted(dynamic):
        add('| `%s` | %d | `%s` | `%s` |' % (path, line, test, arguments.replace('|', r'\|')))
    add('')
    add('## How this was produced')
    add('')
    add('By `auxfiles/list-test-images.py`, which parses the two `TestTools` path helpers out')
    add('of every `.cpp` under `src/tests/` and `src/single_tests/` (excluding')
    add('`testlib/testtools.cpp`, which merely defines them) and records the enclosing test.')
    add('Arguments that are string literals -- including concatenated and `u8`-prefixed ones --')
    add('are resolved against the roots above and stat-ed; anything assembled from a variable is')
    add('listed separately rather than guessed at. A directory argument is marked *(dir)* and')
    add('its size is the sum of the files beneath it.')
    add('')
    add('The document is a snapshot: it reflects the test sources at the commit it was generated')
    add('from and the files present on one machine. Regenerate it rather than editing by hand.')

    io.open(DOC, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print('%s: %d referenced, %d present (%s), %d absent, %d dynamic'
          % (DOC, len(rows), len(present), human(total), len(absent), len(dynamic)))


if __name__ == '__main__':
    main()
