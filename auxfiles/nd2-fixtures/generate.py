#!/usr/bin/env python3
"""Writes the synthetic Nikon ND2 files the ND2 driver tests read.

Written from Nikon_ND2_Format.md (design: software-docs/specs/2026-11-12-nd2-driver-design.md).
Shares no code with the C++ driver or with the validation parser. Deterministic: no
timestamps, no randomness, so a second run changes no byte.

    py -3 auxfiles/nd2-fixtures/generate.py            # writes src/tests/nd2/data/
    py -3 auxfiles/nd2-fixtures/generate.py <out-dir>  # or elsewhere

Stdlib only for the chunked files. The two legacy JPEG 2000 files need `imagecodecs`
and `numpy`; without them they are skipped with a note (they are committed, so CI
never runs this script).

Pixel function (the C++ tests mirror it):

    v = x + 3*y + 17*k + 31*frame + 7*salt      k = component index within the frame
    uint types: v mod 2^bits                    float32: v + 0.5
"""
import os
import struct
import sys
import zlib

MAGIC = 0x0ABECEDA
SIGNATURE_NAME = b'ND2 FILE SIGNATURE CHUNK NAME01!'
FILEMAP_NAME = b'ND2 FILEMAP SIGNATURE NAME 0001!'
CHUNKMAP_SIG = b'ND2 CHUNK MAP SIGNATURE 0000001!'
LEGACY_MAP_SIG = b'LABORATORY IMAGING ND BOX MAP 00'
PAGE = 4096
FRAME_NAME_LENGTH = 4072
PAD_BYTE = b'\xEE'  # row padding (uiWidthBytes beyond the packed width)

DEFAULT_CAL = 0.5
DEFAULT_MAG = 20.0
DEFAULT_JULIAN = 2460000.5
DEFAULT_CHANNEL = ('DAPI', 0x0000FF, 1)


# --------------------------------------------------------------------- pixel function

def sample(x, y, k, frame, salt, bits, is_float):
    v = x + 3 * y + 17 * k + 31 * frame + 7 * salt
    return v + 0.5 if is_float else v % (1 << bits)


# ------------------------------------------------------------------------- CLX trees
# A tree is a list of nodes (kind, name, value); kind is one of
#   'b' bool, 'i32', 'u32', 'u64', 'd' double, 's' string, 'ba' bytes, 'L' level.
# A level's value is a list of child nodes; an empty name marks a list element.

def B(n, v): return ('b', n, v)
def I32(n, v): return ('i32', n, v)
def U32(n, v): return ('u32', n, v)
def U64(n, v): return ('u64', n, v)
def D(n, v): return ('d', n, float(v))
def S(n, v): return ('s', n, v)
def BA(n, v): return ('ba', n, bytes(v))
def L(n, v): return ('L', n, list(v))


_LITE_TYPE = {'b': 1, 'i32': 2, 'u32': 3, 'u64': 5, 'd': 6, 's': 8, 'ba': 9, 'L': 11}


def lite_record(node):
    kind, name, value = node
    head = bytes([_LITE_TYPE[kind], len(name) + 1]) + name.encode('utf-16-le') + b'\0\0'
    if kind == 'b':
        return head + bytes([1 if value else 0])
    if kind == 'i32':
        return head + struct.pack('<i', value)
    if kind == 'u32':
        return head + struct.pack('<I', value)
    if kind == 'u64':
        return head + struct.pack('<Q', value)
    if kind == 'd':
        return head + struct.pack('<d', value)
    if kind == 's':
        return head + value.encode('utf-16-le') + b'\0\0'
    if kind == 'ba':
        return head + struct.pack('<Q', len(value)) + value
    # level: u32 itemCount, u64 indexOffset (from this record's type byte to the index
    # table), the children, then itemCount u64 offsets of the children, measured from
    # this record's type byte
    children = [lite_record(c) for c in value]
    prefix = len(head) + 12
    offsets, pos = [], prefix
    for c in children:
        offsets.append(pos)
        pos += len(c)
    return (head + struct.pack('<IQ', len(children), pos) + b''.join(children)
            + b''.join(struct.pack('<Q', o) for o in offsets))


def lite_chunk(root_name, children):
    """CLX Lite chunk data: one level record that wraps the content."""
    return lite_record(L(root_name, children))


def lite_compressed(root_name, children):
    """The same tree as a type-76 record: 2 header bytes + 10 skipped bytes, then zlib."""
    raw = lite_record(L(root_name, children))
    return bytes([76, 0]) + struct.pack('<Q', len(raw)) + b'\0\0' + zlib.compress(raw, 9)


def _xml_attr(text):
    out = []
    for ch in text:
        if ch == '&': out.append('&amp;')
        elif ch == '<': out.append('&lt;')
        elif ch == '>': out.append('&gt;')
        elif ch == '"': out.append('&quot;')
        elif ch in '\r\n\t': out.append('&#%d;' % ord(ch))
        else: out.append(ch)  # control characters such as \x01 are left raw on purpose
    return ''.join(out)


_XML_RUNTYPE = {'b': 'bool', 'i32': 'lx_int32', 'u32': 'lx_uint32', 'u64': 'lx_uint64',
                'd': 'double', 's': 'CLxStringW', 'ba': 'CLxByteArray'}


def _xml_node(node):
    kind, name, value = node
    tag = name or 'no_name'
    if kind == 'L':
        return '<%s runtype="CLxListVariant">%s</%s>' % (tag, ''.join(_xml_node(c) for c in value), tag)
    if kind == 'b':
        text = 'true' if value else 'false'
    elif kind == 'd':
        text = repr(value)
    elif kind == 'ba':
        text = value.decode('latin-1')
    else:
        text = str(value)
    return '<%s runtype="%s" value="%s"/>' % (tag, _XML_RUNTYPE[kind], _xml_attr(text))


def xml_chunk(children):
    """CLX XML chunk data: <variant><no_name>content</no_name></variant>."""
    body = _xml_node(L('', children))
    return ('<?xml version="1.0" encoding="UTF-8"?><variant version="1.0">%s</variant>' % body).encode('utf-8')


# -------------------------------------------------------------------------- the spec

class Spec:
    """Everything one chunked fixture varies."""

    def __init__(self, **kw):
        self.name = None
        self.version = (3, 0)
        self.width, self.height = 16, 12
        self.comp = 1
        self.bits = 8
        self.is_float = False
        self.sig_bits = None            # uiBpcSignificant, default = bits
        self.width_bytes = None         # default: packed width rounded up to 4
        self.channels = [DEFAULT_CHANNEL]
        self.loops = []                 # outermost first; see loop_children()
        self.frames = None              # frame indices present (default 0..n-1)
        self.frame_count = None         # number of frames the loops describe
        self.acq_count = None           # entries of AcqTimesCache; None = no chunk
        self.sequence_count = None
        self.aligned = True
        self.frame_name_length = FRAME_NAME_LENGTH   # None = bare name
        self.bare_names = True          # metadata chunk names carry no padding
        self.ecomp = 2
        self.zlib = False
        self.tile = None                # (w, h) override
        self.text_info = False
        self.control_char = False       # \x01 inside TextInfoItem_5
        self.wrap_pars = False          # XML: uLoopPars wrapped in i0000000000
        self.compress_picture = False   # ImageMetadataSeqLV|0! as a type-76 tree
        self.acq_before_frames = False
        self.stale = ()                 # frame indices whose map entry points 4096 too early
        self.truncate_frame = None      # frame whose declared dataLength runs past EOF
        self.salt = 0
        self.__dict__.update(kw)
        if self.sig_bits is None:
            self.sig_bits = self.bits
        if self.width_bytes is None:
            packed = self.width * self.comp * self.bits // 8
            self.width_bytes = (packed + 3) // 4 * 4
        if self.frame_count is None:
            n = 1
            for lp in self.loops:
                n *= loop_count(lp)
            self.frame_count = n
        if self.frames is None:
            self.frames = list(range(self.frame_count))
        if self.sequence_count is None:
            self.sequence_count = self.frame_count

    @property
    def xml(self):
        return self.version[0] < 3

    @property
    def total_components(self):
        return self.comp


def loop_count(lp):
    t = lp['t']
    if t == 2:
        return sum(lp.get('valid', [1] * lp['n']))
    if t == 8:
        return sum(p['n'] for p, ok in zip(lp['periods'], lp['valid']) if ok)
    if t == 6:
        return 1   # spectral: channels live inside each frame
    return lp['n']


# ----------------------------------------------------------------- metadata builders

def attributes_children(s):
    tw, th = s.tile if s.tile else (s.width, s.height)
    out = [U32('uiWidth', s.width), U32('uiWidthBytes', s.width_bytes), U32('uiHeight', s.height),
           U32('uiComp', s.comp), U32('uiBpcInMemory', s.bits), U32('uiBpcSignificant', s.sig_bits),
           U32('uiSequenceCount', s.sequence_count), U32('uiTileWidth', tw), U32('uiTileHeight', th),
           U32('eCompression', s.ecomp), D('dCompressionParam', 1.0 if s.zlib or s.ecomp < 2 else 0.0)]
    return out


def _time_pars(n, period, start=0.0):
    return [U32('uiCount', n), D('dStart', start), D('dPeriod', period), D('dDuration', period * n),
            D('dMinPeriodDiff', 0.0), D('dMaxPeriodDiff', 0.0), D('dAvgPeriodDiff', period)]


def _loop_pars(lp):
    t = lp['t']
    if t == 1:
        return _time_pars(lp['n'], lp.get('period', 100.0))
    if t == 2:
        names = lp.get('names', [''] * lp['n'])
        points = [L('', [D('dPosX', 1000.0 * i), D('dPosY', -500.0 * i), D('dPosZ', 10.0 * i),
                         D('dPFSOffset', -1.0), S('dPosName', names[i])]) for i in range(lp['n'])]
        return [U32('uiCount', lp['n']), B('bUseZ', False), B('bRelativeXY', False),
                D('dReferenceX', 0.0), D('dReferenceY', 0.0), L('Points', points)]
    if t == 4:
        step = lp.get('step', 1.0)
        n = lp['n']
        return [U32('uiCount', n), D('dZLow', 0.0), D('dZHigh', step * (n - 1)), D('dZHome', 0.0),
                D('dZStep', step), B('bZInverted', False), I32('iType', 0), S('wsZDevice', 'Fixture Z')]
    if t == 6:
        return [U32('uiCount', 1), L('pPlanes', [U32('uiCount', 1)])]
    if t == 7:
        return [U32('uiCount', lp['n'])]
    if t == 8:
        periods = [L('', _time_pars(p['n'], p.get('period', 100.0))) for p in lp['periods']]
        return [L('pPeriod', periods), BA('pPeriodValid', lp['valid'])]
    raise ValueError(t)


def loop_children(loops, wrap_pars=False):
    """Fields of a loop record: the first of `loops`, with the rest nested below it."""
    if not loops:
        return [U32('eType', 1), L('uLoopPars', [U32('uiCount', 0)]), U32('uiNextLevelCount', 0)]
    lp = loops[0]
    pars = _loop_pars(lp)
    if wrap_pars:
        pars = [L('i0000000000', pars)]
    out = [U32('eType', lp['t']), L('uLoopPars', pars)]
    if lp['t'] == 2:
        out.append(BA('pItemValid', lp.get('valid', [1] * lp['n'])))
    rest = loops[1:]
    out.append(U32('uiNextLevelCount', 1 if rest else 0))
    if rest:
        out.append(L('ppNextLevelEx', [L('', loop_children(rest, wrap_pars))]))
    out.append(U32('uiRepeatCount', loop_count(lp) if lp['t'] != 6 else 1))
    return out


def picture_children(s):
    planes = []
    for i, (name, color, comps) in enumerate(s.channels):
        planes.append(L('a%d' % i, [U32('uiSampleIndex', i), U32('uiCompCount', comps),
                                    S('sDescription', name), U32('uiColor', color),
                                    L('pFluorescentProbe', [S('m_sName', name)])]))
    plane_key = 'sPlane' if s.xml and s.version < (2, 1) else 'sPlaneNew'
    return [D('dTimeMSec', 0.0), D('dTimeAbsolute', DEFAULT_JULIAN),
            D('dXPos', 0.0), D('dYPos', 0.0), D('dZPos', 0.0),
            D('dCalibration', DEFAULT_CAL), D('dAspect', 1.0), B('bCalibrated', True),
            S('wsObjectiveName', 'Plan Apo 20x'), D('dObjectiveMag', DEFAULT_MAG), D('dObjectiveNA', 0.75),
            D('dRefractIndex1', 1.0), D('dZoom', 1.0), D('dProjectiveMag', -1.0),
            U32('ePictureXAxis', 0), U32('ePictureYAxis', 0),
            L('sPicturePlanes', [U32('uiCount', len(s.channels)), U32('uiSampleCount', len(s.channels)),
                                 L(plane_key, planes)])]


def text_info_children(s):
    desc = 'Synthetic ND2 fixture'
    if s.control_char:
        desc = 'Synthetic\x01ND2 fixture'
    return [S('TextInfoItem_5', desc), S('TextInfoItem_6', 'Fixture capture settings'),
            S('TextInfoItem_13', 'Plan Apo 20x')]


# ------------------------------------------------------------------ chunk assembly

def frame_pixels(s, frame):
    bps = s.bits // 8
    fmt = 'f' if s.is_float else {8: 'B', 16: 'H'}[s.bits]
    pad = s.width_bytes - s.width * s.comp * bps
    rows = []
    for y in range(s.height):
        vals = [sample(x, y, k, frame, s.salt, s.bits, s.is_float)
                for x in range(s.width) for k in range(s.comp)]
        rows.append(struct.pack('<%d%s' % (len(vals), fmt), *vals) + PAD_BYTE * pad)
    return b''.join(rows)


def frame_data(s, frame):
    pixels = frame_pixels(s, frame)
    if s.zlib:
        pixels = zlib.compress(pixels, 1)   # header 78 01
    return struct.pack('<d', frame * 100.0) + pixels


def chunk_bytes(name, data, name_length, declared=None):
    field = name + b'\0' * (name_length - len(name))
    return struct.pack('<IIQ', MAGIC, name_length, len(data) if declared is None else declared) + field + data


def build_chunked(s, declared=None):
    """Returns the file bytes. `declared` overrides the truncated frame's dataLength."""
    buf = bytearray()
    entries = []   # (name, offset, size)

    def place(name, data, name_length=None, frame=None):
        if s.aligned and len(buf) % PAGE:
            buf.extend(b'\0' * (PAGE - len(buf) % PAGE))
        off = len(buf)
        nl = len(name) if name_length is None else name_length
        dl = declared if (frame is not None and frame == s.truncate_frame and declared is not None) else None
        buf.extend(chunk_bytes(name, data, nl, dl))
        entries.append((name, off, len(data) if dl is None else dl))
        return off

    sig = ('Ver%d.%d' % s.version).encode('ascii').ljust(64, b'\0')
    buf.extend(chunk_bytes(SIGNATURE_NAME, sig, 32))

    lv = '' if s.xml else 'LV'

    def meta(base, root, children, compress=False):
        # 3.x: ...LV chunks in CLX Lite; 2.x: plain chunks in CLX XML
        suffix = '|0!' if base.endswith('|0') else '!'
        name = (base[:-2] if suffix == '|0!' else base) + lv + suffix
        if s.xml:
            data = xml_chunk(children)
        elif compress:
            data = lite_compressed(root, children)
        else:
            data = lite_chunk(root, children)
        return name.encode(), data

    n, d = meta('ImageAttributes', 'SLxImageAttributes', attributes_children(s))
    place(n, d)
    n, d = meta('ImageMetadata', 'SLxExperiment', loop_children(s.loops, s.wrap_pars))
    place(n, d)
    n, d = meta('ImageMetadataSeq|0', 'SLxPictureMetadata', picture_children(s), s.compress_picture)
    place(n, d)
    if s.text_info:
        n, d = meta('ImageTextInfo', 'SLxImageTextInfo', text_info_children(s))
        place(n, d)

    def acq():
        if s.acq_count is not None:
            place(b'CustomData|AcqTimesCache!',
                  b''.join(struct.pack('<d', i * 100.0) for i in range(s.acq_count)))

    if s.acq_before_frames:
        acq()
    for f in s.frames:
        place(('ImageDataSeq|%d!' % f).encode(), frame_data(s, f), s.frame_name_length, frame=f)
    if not s.acq_before_frames:
        acq()

    # chunk map, last. Entries: name, u64 offset, u64 size. The terminating entry is
    # also the file's 40-byte trailer.
    if s.aligned and len(buf) % PAGE:
        buf.extend(bytes(PAGE - len(buf) % PAGE))
    map_offset = len(buf)
    body = bytearray()
    for name, off, size in entries:
        if name.startswith(b'ImageDataSeq|') and int(name[13:-1]) in s.stale:
            off -= PAGE
        body += name + struct.pack('<QQ', off, size)
    body += CHUNKMAP_SIG + struct.pack('<Q', map_offset)
    buf.extend(chunk_bytes(FILEMAP_NAME, bytes(body), 32))
    return bytes(buf), entries


def write_chunked(path, s):
    data, entries = build_chunked(s)
    if s.truncate_frame is not None:
        # frame data start = chunk offset + 16 + nameLength; declare 1000 bytes more than
        # the real bytes from there to EOF. The layout does not depend on the declared value.
        off = next(o for n, o, _ in entries if n == ('ImageDataSeq|%d!' % s.truncate_frame).encode())
        real = len(data) - (off + 16 + s.frame_name_length)
        data, _ = build_chunked(s, declared=real + 1000)
    with open(path, 'wb') as f:
        f.write(data)


# --------------------------------------------------------------------- legacy JP2

def _box(kind, payload):
    return struct.pack('>I4s', 8 + len(payload), kind) + payload


def _lim_xml(children):
    """A legacy box body: LIM comment, then a CLX XML variant (no declaration)."""
    return (b'<!--Laboratory Imaging(http://www.lim.cz) synthetic fixture-->'
            + ('<variant version="1.0">%s</variant>' % _xml_node(L('', children))).encode('utf-8'))


def _text_item(text):
    return L('TextInfoItem', [S('Text', text)])


def write_legacy(path, width, height, bits, channels, frames, timepoints_xy, loops_xml, dims_text,
                 missing=(), calibration=0.25):
    """frames: number of frames; missing: (frame, channel) pairs with no codestream."""
    import numpy as np
    import imagecodecs

    dtype = np.uint8 if bits == 8 else np.uint16
    boxes = []   # (box type, lim type, bytes)
    boxes.append((b'jP  ', b'LUNK', b'\x00\x00\x00\x0cjP  \r\n\x87\n'))
    boxes.append((b'ftyp', b'LUNK', _box(b'ftyp', b'jp2 ' + struct.pack('>I', 0) + b'jp2 ')))
    ihdr = struct.pack('>IIHBBBB', height, width, 1, bits - 1, 7, 0, 0)
    colr = struct.pack('>BBBI', 1, 0, 0, 17)
    boxes.append((b'jp2h', b'LUNK', _box(b'jp2h', _box(b'ihdr', ihdr) + _box(b'colr', colr))))

    ys, xs = np.mgrid[0:height, 0:width]
    plane_children = [L('a%d' % c, [U32('CompCount', 1), S('OpticalConfigName', 'CH%d' % c),
                                    U32('Color', 0x0000FF if c == 0 else 0x00FF00)])
                      for c in range(channels)]
    for f in range(frames):
        for c in range(channels):
            if (f, c) in missing:
                continue
            arr = ((xs + 3 * ys + 17 * c + 31 * f) % (1 << bits)).astype(dtype)
            stream = imagecodecs.jpeg2k_encode(arr, level=0, reversible=True, codecformat='J2K')
            boxes.append((b'jp2c', b'LUNK', _box(b'jp2c', stream)))
        vimd = [D('TimeMSec', f * 100.0), D('TimeAbsolute', DEFAULT_JULIAN), D('XPos', 0.0), D('YPos', 0.0),
                D('ZPos', 0.0), D('Calibration', calibration), D('Aspect', 1.0), B('Calibrated', True),
                S('ObjectiveName', 'Plan Apo 20x'), D('ObjectiveMag', DEFAULT_MAG), D('ObjectiveNA', 0.75),
                D('RefractIndex1', 1.0), D('ProjectiveMag', -1.0),
                L('PicturePlanes', [U32('Count', channels), U32('CompCount', channels),
                                    L('Plane', plane_children)])]
        boxes.append((b'xml ', b'VIMD', _box(b'xml ', _lim_xml(vimd))))
        vcal = [D('Calibration', calibration), D('Aspect', 1.0), B('Calibrated', True)]
        boxes.append((b'xml ', b'VCAL', _box(b'xml ', _lim_xml(vcal))))
    boxes.append((b'xml ', b'AIM1', _box(b'xml ', _lim_xml(loops_xml))))
    tinf = [_text_item('Synthetic ND2 fixture'),
            _text_item('Metadata:\r\nDimensions: %s\r\nCamera: fixture\r\n' % dims_text)]
    boxes.append((b'xml ', b'TINF', _box(b'xml ', _lim_xml(tinf))))

    # offsets of all boxes, then the final uuid box carries the map and the trailer
    offsets, pos = [], 0
    for _, _, b in boxes:
        offsets.append(pos)
        pos += len(b)
    uuid_offset = pos
    count = len(boxes) + 1
    uuid_id = bytes(range(0xA0, 0xB0))
    map_bytes = struct.pack('>I', count)
    for (bt, lt, _), off in zip(boxes, offsets):
        map_bytes += struct.pack('<4s4sQ', bt, lt, off)
    map_bytes += struct.pack('<4s4sQ', b'uuid', b'PMAP', uuid_offset)
    uuid_len = 8 + 16 + len(map_bytes) + 40
    file_size = uuid_offset + uuid_len
    map_start = uuid_offset + 8 + 16
    trailer = LEGACY_MAP_SIG + struct.pack('<Q', file_size - map_start)
    uuid_box = struct.pack('>I4s', uuid_len, b'uuid') + uuid_id + map_bytes + trailer
    with open(path, 'wb') as f:
        for _, _, b in boxes:
            f.write(b)
        f.write(uuid_box)
    assert os.path.getsize(path) == file_size


def _legacy_time_loop(count):
    return [U32('Type', 8),
            L('LoopPars', [U32('Count', count), U32('PeriodCount', 1),
                           L('Period', [L('a0', [U32('Count', count), D('Start', 0.0), D('Period', 100.0),
                                                 D('Duration', 100.0 * count), D('AvgPeriodDiff', 100.0)])])])]


def _legacy_xy_loop(count):
    return [U32('Type', 2),
            L('LoopPars', [U32('Count', count), B('UseZ', False),
                           L('PosX', [D('a%05d' % i, 1000.0 * i) for i in range(count)]),
                           L('PosY', [D('a%05d' % i, -500.0 * i) for i in range(count)]),
                           L('PosZ', [D('a%05d' % i, 0.0) for i in range(count)]),
                           L('PFSOffset', [D('a%05d' % i, -1.0) for i in range(count)])]),
            L('ItemValid', [B('a%05d' % i, True) for i in range(count)]),
            U32('RepeatCount', count)]


def legacy_loops_tchannel(t):
    spectral = [U32('Type', 6), L('LoopPars', [U32('Count', 2)])]
    out = _legacy_time_loop(t)
    out.append(L('NextLevelEx', spectral))
    return out


def legacy_loops_t_xy(t, xy):
    out = _legacy_time_loop(t)
    out.append(L('NextLevelEx', _legacy_xy_loop(xy)))
    return out


# ------------------------------------------------------------------------- the files

def T(n, period=100.0): return {'t': 1, 'n': n, 'period': period}
def Z(n, step=1.0): return {'t': 4, 'n': n, 'step': step}
def XY(n, valid=None, names=None):
    d = {'t': 2, 'n': n, 'valid': valid or [1] * n}
    if names:
        d['names'] = names
    return d


def chunked_specs():
    two = [('DAPI', 0x0000FF, 1), ('GFP', 0x00FF00, 1)]
    specs = [
        Spec(name='v3_basic', frames=[0], frame_count=1, acq_count=1),
        Spec(name='v3_tpz', bits=16, sig_bits=12, comp=2, channels=two, text_info=True,
             loops=[T(3, 2000.0), XY(3, [1, 0, 1], ['A', 'skip', 'A']), Z(4, 1.5)],
             frame_count=24, acq_count=24, compress_picture=True),
        Spec(name='order_pz', loops=[XY(2), Z(3)], acq_count=6),
        Spec(name='order_zt', loops=[Z(2), T(3)], acq_count=6),
        Spec(name='v21_xml', version=(2, 1), aligned=False, width=15, height=5, comp=3, width_bytes=48,
             channels=[('RGB', 0x0000FF, 3)], loops=[T(2), Z(3)], acq_count=6, text_info=True,
             control_char=True, wrap_pars=True),
        Spec(name='v20_unpadded', version=(2, 0), aligned=False, frame_name_length=None, width=7, height=4,
             bits=16, width_bytes=16, loops=[Z(3)], acq_count=None),
        Spec(name='v20_zlib', version=(2, 0), aligned=False, frame_name_length=None, width=7, height=4,
             bits=16, width_bytes=16, loops=[Z(3)], acq_count=None, ecomp=0, zlib=True),
        Spec(name='stale_map', loops=[Z(5)], acq_count=5, stale=(1, 3)),
        Spec(name='frames_missing', loops=[T(5), Z(2)], frames=[0, 1, 2, 4, 5], acq_count=6),
        Spec(name='frames_surplus', loops=[Z(3)], frames=[0, 1, 2, 3, 4], acq_count=5),
        Spec(name='ne_time', frame_count=4, acq_count=4,
             loops=[{'t': 8, 'periods': [{'n': 2}, {'n': 5}], 'valid': [1, 0]}, {'t': 6}, Z(2)]),
        Spec(name='float32', bits=32, is_float=True, frames=[0], frame_count=1, acq_count=1),
        Spec(name='multi_rgb', width=6, height=4, comp=12, width_bytes=72,
             channels=[('BF%d' % i, 0x0000FF, 3) for i in range(1, 5)], frames=[0], frame_count=1, acq_count=1),
        Spec(name='truncated_frame', loops=[Z(3)], acq_count=3, acq_before_frames=True, truncate_frame=2),
    ]
    # refusals: one frame each keeps them small
    one = dict(frames=[0], frame_count=1, acq_count=1)
    specs += [
        Spec(name='bad_width', width=0, width_bytes=0, **one),
        Spec(name='lossy', ecomp=1, **one),
        Spec(name='tiled', tile=(8, 12), **one),
        Spec(name='custom_loop', loops=[{'t': 7, 'n': 3}], frames=[0, 1, 2], frame_count=3, acq_count=3),
        Spec(name='two_time_loops', loops=[T(2), T(3)], frames=[0], frame_count=6, acq_count=1),
        Spec(name='bad_comp', comp=4, channels=[('C%d' % i, 0x0000FF, 1) for i in range(3)], **one),
    ]
    return specs


def main():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'src', 'tests', 'nd2', 'data')
    os.makedirs(out, exist_ok=True)
    written = 0
    for s in chunked_specs():
        write_chunked(os.path.join(out, s.name + '.nd2'), s)
        written += 1
    with open(os.path.join(out, 'not_nd2.nd2'), 'wb') as f:
        f.write(b'\x41' * 64)
    written += 1
    try:
        import imagecodecs  # noqa: F401
        import numpy  # noqa: F401
    except ImportError:
        print('imagecodecs/numpy not available: legacy.nd2 and legacy_xy.nd2 skipped '
              '(the committed copies stay as they are)')
    else:
        write_legacy(os.path.join(out, 'legacy.nd2'), 8, 6, 16, 2, 3, None, legacy_loops_tchannel(3),
                     'T(3) x λ(2)', missing=((2, 1),))
        write_legacy(os.path.join(out, 'legacy_xy.nd2'), 8, 6, 8, 1, 4, None, legacy_loops_t_xy(2, 4),
                     'T(2) x XY(2)')
        written += 2
    print('wrote %d files to %s' % (written, out))


if __name__ == '__main__':
    main()
