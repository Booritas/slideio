#!/usr/bin/env python3
"""Synthetic Leica LIF fixtures for the slideio LIF driver tests.

Stdlib only.  Writes src/tests/lif/data/*.lif (or the directory given as the
first argument).  The output is deterministic: running it twice writes the
same bytes.  Shares no code with the C++ driver.

Pixel function (the C++ tests mirror it):

    v = x + 3*y + 17*c + 31*z + 59*t + 97*m + 7*salt
    uint types: v mod 2^bits          float types: v + 0.5

c is the stored channel index (description order), salt the image's 0-based
index in its file (document order).  Each axis uses its own identity: in an XZ
image the second raster axis is z (y is 0), in an XT image it is t.  For
interleaved images c is the sample index inside the pixel.  An axis the
function has no term for (DimID 5, 9, ...) contributes 0.  "bits" is the
storage width, not the declared Resolution.
"""
import struct
import sys
from pathlib import Path
from xml.sax.saxutils import quoteattr

FILETIME_BASE = 131000000000000000
FILETIME_STEP = 10000000

DIM_X, DIM_Y, DIM_Z, DIM_T, DIM_M = 1, 2, 3, 4, 10
ROLE_OF_DIMID = {DIM_X: 'x', DIM_Y: 'y', DIM_Z: 'z', DIM_T: 't', DIM_M: 'm'}

# type name -> (storage bytes, DataType attribute, Resolution, struct code or None for uint)
TYPES = {
    'u8': (1, 0, 8, None),
    'u16': (2, 0, 16, None),
    'u16_12': (2, 0, 12, None),
    'u32': (4, 0, 32, None),
    'f16': (2, 1, 16, 'e'),
    'f32': (4, 1, 32, 'f'),
    'f64': (8, 1, 64, 'd'),
}


class Dim:
    def __init__(self, dimid, n, stride, length=None, unit=None):
        self.dimid, self.n, self.stride = dimid, n, stride
        self.length, self.unit = length, unit
        self.role = ROLE_OF_DIMID.get(dimid)


class Chan:
    def __init__(self, stride, tag=None):
        # tag None: ChannelTag="0" (gray); 'none': the attribute is omitted
        self.stride, self.tag = stride, tag


class Image:
    def __init__(self, name, dims, chans, dtype='u8', datatype_attr=True,
                 resolution=None, sample_bytes=None, sample_code=None,
                 payload_len=None, block_id=None, declared_size=None,
                 write_block=True, stamps=None, attachment=False):
        self.name, self.dims, self.chans = name, dims, chans
        self.dtype = dtype
        self.datatype_attr = datatype_attr    # False: omit DataType
        self.resolution = resolution          # override of the declared Resolution
        self.sample_bytes = sample_bytes      # storage width actually written
        self.sample_code = sample_code        # 'u' or a struct code actually written
        self.payload_len = payload_len        # truncate / extend the payload
        self.block_id = block_id              # override of MemBlock_<salt>
        self.declared_size = declared_size    # override of Memory/@Size
        self.write_block = write_block
        self.stamps = stamps                  # (kind, [ticks])
        self.attachment = attachment
        self.salt = 0
        self.payload = b''

    @property
    def mem_id(self):
        return self.block_id or 'MemBlock_%d' % self.salt


def default_dims(w=16, h=12, bpp=1):
    return [Dim(DIM_X, w, bpp, 1.5e-5, 'm'), Dim(DIM_Y, h, w * bpp, 1.1e-5, 'm')]


def simple(name, dtype='u8', w=16, h=12, **kw):
    return Image(name, default_dims(w, h, TYPES[dtype][0]), [Chan(0)], dtype=dtype, **kw)


def build_payload(img):
    bpp, _, _, code = TYPES[img.dtype]
    if img.sample_bytes is not None:
        bpp = img.sample_bytes
    if img.sample_code is not None:
        code = None if img.sample_code == 'u' else img.sample_code
    bits = 8 * bpp
    extent = sum((d.n - 1) * d.stride for d in img.dims)
    extent += max(c.stride for c in img.chans) + bpp
    buf = bytearray(extent)
    axes = [(d.role, d.n, d.stride) for d in img.dims]

    def rec(i, base, idx):
        if i == len(axes):
            for ci, ch in enumerate(img.chans):
                v = (idx.get('x', 0) + 3 * idx.get('y', 0) + 17 * ci +
                     31 * idx.get('z', 0) + 59 * idx.get('t', 0) +
                     97 * idx.get('m', 0) + 7 * img.salt)
                if code:
                    raw = struct.pack('<' + code, v + 0.5)
                else:
                    raw = (v % (1 << bits)).to_bytes(bpp, 'little')
                off = base + ch.stride
                buf[off:off + bpp] = raw
            return
        role, n, stride = axes[i]
        for k in range(n):
            if role is not None:
                idx[role] = k
            rec(i + 1, base + k * stride, idx)
        if role is not None:
            idx.pop(role, None)

    rec(0, 0, {})
    data = bytes(buf)
    if img.payload_len is not None:
        data = data[:img.payload_len] + bytes(max(0, img.payload_len - len(data)))
    return data


def image_xml(img):
    _, dt, res, _ = TYPES[img.dtype]
    if img.resolution is not None:
        res = img.resolution
    out = ['<Element Name=%s><Data><Image><ImageDescription><Channels>' % quoteattr(img.name)]
    for ch in img.chans:
        # The attributes after LUTName are not read by the driver; real files
        # carry them and liffile refuses a channel without them.
        a = ['LUTName="Gray"', 'NameOfMeasuredQuantity=""', 'Min="0"', 'Max="0"',
             'Unit=""', 'IsLUTInverted="0"', 'BitInc="0"']
        if img.datatype_attr:
            a.append('DataType="%d"' % dt)
        a.append('Resolution="%d"' % res)
        if ch.tag != 'none':
            a.append('ChannelTag="%d"' % (ch.tag or 0))
        a.append('BytesInc="%d"' % ch.stride)
        out.append('<ChannelDescription %s/>' % ' '.join(a))
    out.append('</Channels><Dimensions>')
    for d in img.dims:
        a = ['DimID="%d"' % d.dimid, 'NumberOfElements="%d"' % d.n,
             'Origin="0"', 'BytesInc="%d"' % d.stride, 'BitInc="0"']
        if d.length is not None:
            a.append('Length=%s' % quoteattr(str(d.length)))
        if d.unit is not None:
            a.append('Unit=%s' % quoteattr(d.unit))
        out.append('<DimensionDescription %s/>' % ' '.join(a))
    out.append('</Dimensions></ImageDescription>')
    if img.stamps:
        kind, ticks = img.stamps
        if kind == 'hex':
            out.append('<TimeStampList NumberOfTimeStamps="%d">%s</TimeStampList>'
                       % (len(ticks), ' '.join('%x' % t for t in ticks)))
        else:
            out.append('<TimeStampList>')
            for t in ticks:
                out.append('<TimeStamp HighInteger="%d" LowInteger="%d"/>'
                           % (t >> 32, t & 0xFFFFFFFF))
            out.append('</TimeStampList>')
    if img.attachment:
        out.append('<Attachment Name="HardwareSetting">'
                   '<ATLConfocalSettingDefinition Magnification="63"/></Attachment>')
    out.append('</Image></Data>')
    size = img.declared_size if img.declared_size is not None else len(img.payload)
    out.append('<Memory MemoryBlockID=%s Size="%d"/></Element>' % (quoteattr(img.mem_id), size))
    return ''.join(out)


def header(xml):
    n = len(xml)
    return struct.pack('<IIBI', 0x70, 2 * n + 5, 0x2A, n) + xml.encode('utf-16-le')


def block(bid, payload, version, declared_len=None):
    lw = 'I' if version == 1 else 'Q'
    length = len(payload) if declared_len is None else declared_len
    return (struct.pack('<IIB' + lw + 'BI', 0x70, 0, 0x2A, length, 0x2A, len(bid)) +
            bid.encode('utf-16-le') + payload)


def container_xml(nodes, version):
    """nodes: Image or ('folder', name, [nodes])."""
    def render(n):
        if isinstance(n, Image):
            return image_xml(n)
        return ('<Element Name=%s><Data/><Children>%s</Children></Element>'
                % (quoteattr(n[1]), ''.join(render(c) for c in n[2])))
    return ('<LMSDataContainerHeader Version="%d"><Element Name="Fixture"><Data/>'
            '<Children>%s</Children></Element></LMSDataContainerHeader>'
            % (version, ''.join(render(n) for n in nodes)))


def flatten(nodes):
    for n in nodes:
        if isinstance(n, Image):
            yield n
        else:
            yield from flatten(n[2])


def prepare(nodes):
    imgs = list(flatten(nodes))
    for i, img in enumerate(imgs):
        img.salt = i
        img.payload = build_payload(img)
    return imgs


def make_lif(nodes, version=2, order=None, before=(), after=(), trailer=b'\0\0\0\0',
             declare_extra=None, duplicate_of=None):
    """before/after: (id, payload) blocks around the image blocks.
    order: image names in the order their blocks are written.
    declare_extra: (image name, n) -- that block declares n more bytes than it holds.
    duplicate_of: image name; a second block with the same id, 0xFF filled,
    is written straight after the real one."""
    imgs = prepare(nodes)
    out = bytearray(header(container_xml(nodes, version)))
    for bid, p in before:
        out += block(bid, p, version)
    writ = imgs
    if order:
        writ = [next(i for i in imgs if i.name == n) for n in order]
    for img in writ:
        if not img.write_block:
            continue
        extra = declare_extra[1] if declare_extra and declare_extra[0] == img.name else 0
        out += block(img.mem_id, img.payload, version, len(img.payload) + extra)
        if duplicate_of == img.name:
            out += block(img.mem_id, b'\xff' * len(img.payload), version)
    for bid, p in after:
        out += block(bid, p, version)
    out += trailer
    return bytes(out)


# ---------------------------------------------------------------- fixtures

def basic(version):
    return make_lif([simple('Plain', attachment=True)], version,
                    before=[('MemBlock_empty', b'')],
                    after=[('MemBlock_extra', bytes(range(64)))])


def nesting():
    w, h = 8, 6
    sizes = {'C': 2, 'Z': 3, 'M': 4, 'T': 2}
    plane = w * h * 2
    imgs = []
    for order in ('CZMT', 'ZCMT', 'MCZT', 'CZTM'):
        stride, s = {}, plane
        for a in order:
            stride[a] = s
            s *= sizes[a]
        dims = [Dim(DIM_X, w, 2, 1.5e-5, 'm'), Dim(DIM_Y, h, w * 2, 1.1e-5, 'm'),
                Dim(DIM_Z, 3, stride['Z'], 4e-6, 'm'),
                Dim(DIM_T, 2, stride['T'], 10, 's'),
                Dim(DIM_M, 4, stride['M'], 0, '')]
        chans = [Chan(k * stride['C']) for k in range(2)]
        imgs.append(Image(order, dims, chans, dtype='u16'))
    return make_lif([('folder', 'Nest', imgs)])


def types():
    return make_lif([
        simple('U8', 'u8'),
        simple('U16_12bit', 'u16_12'),
        simple('U32', 'u32'),
        simple('F16', 'f16'),
        simple('F32', 'f32'),
        simple('F64', 'f64'),
        simple('NoDataType16', 'u16', datatype_attr=False),
        # DataType=1 Resolution=32 declares float32 but X stride 2 stores uint16
        Image('Mismatch', default_dims(16, 12, 2), [Chan(0)], dtype='f32',
              sample_bytes=2, sample_code='u'),
    ])


def rgb():
    def im(name, tags):
        dims = [Dim(DIM_X, 8, 3, 1.5e-5, 'm'), Dim(DIM_Y, 6, 24, 1.1e-5, 'm')]
        return Image(name, dims, [Chan(i, t) for i, t in enumerate(tags)])
    return make_lif([im('TagRGB', (1, 2, 3)), im('TagBGR', (3, 2, 1)),
                     im('Untagged', ('none', 'none', 'none'))])


def xzxt():
    xz = Image('XZ', [Dim(DIM_X, 16, 1, 1.5e-5, 'm'), Dim(DIM_Z, 5, 16, 5e-6, 'm')], [Chan(0)])
    xt = Image('XT', [Dim(DIM_X, 16, 1, 1.5e-5, 'm'), Dim(DIM_T, 7, 16, 7, 's')], [Chan(0)])
    return make_lif([xz, xt])


def extra_axes():
    plane = 16 * 12
    e1 = Image('Extra1', default_dims() + [Dim(5, 1, plane, 5e-7, 'm')], [Chan(0)])
    lam = Image('Lambda', default_dims() + [Dim(9, 12, plane, 1.2e-7, 'm')], [Chan(0)])
    return make_lif([e1, lam, simple('Plain')])


def all_unsupported():
    plane = 16 * 12
    return make_lif([Image('Lambda', default_dims() + [Dim(9, 12, plane, 1.2e-7, 'm')], [Chan(0)])])


def padded():
    return make_lif([Image('Padded', [Dim(DIM_X, 16, 1, 1.5e-5, 'm'),
                                      Dim(DIM_Y, 12, 16 + 4, 1.1e-5, 'm')], [Chan(0)])])


def bad_strides():
    plane = 16 * 12
    # Z stride 100 < plane extent 192: planes alias
    alias = Image('Alias', default_dims() + [Dim(DIM_Z, 3, 100, 3e-6, 'm')], [Chan(0)])
    # T stride 1000 with a payload holding two planes: plane 1 is out of bounds
    oob = Image('OOB', default_dims() + [Dim(DIM_T, 2, 1000, 2, 's')], [Chan(0)],
                payload_len=2 * plane)
    return make_lif([alias, oob, simple('Plain')])


def broken_refs():
    return make_lif([
        simple('DupRef'),                      # MemBlock_0 appears on two blocks
        simple('Missing', block_id='MemBlock_missing', write_block=False),
        simple('SizeMismatch', declared_size=16 * 12 + 1),
        simple('Plain'),
    ], duplicate_of='DupRef')


def truncated():
    return make_lif([simple('Good'), simple('Cut')], trailer=b'',
                    declare_extra=('Cut', 1000))


def unicode_():
    return make_lif([simple('Zellkern µ – 細胞')])


def timestamps():
    plane = 16 * 12

    def im(name, kind, n):
        # C innermost (stride = plane bytes), then T: storage index k = c + 2*t
        dims = default_dims() + [Dim(DIM_T, 3, 2 * plane, 3, 's')]
        ticks = [FILETIME_BASE + k * FILETIME_STEP for k in range(6)][:n]
        return Image(name, dims, [Chan(0), Chan(plane)], stamps=(kind, ticks))
    return make_lif([im('Hex', 'hex', 6), im('Decimal', 'dec', 6), im('Short', 'hex', 5)])


def lof():
    return header('<LMS_Object_File Version="2"><Element Name="Fixture"><Data/></Element>'
                  '</LMS_Object_File>')


FIXTURES = {
    'v2_basic': lambda: basic(2),
    'v1_basic': lambda: basic(1),
    'v3_probe': lambda: basic(3),
    'order_by_id': lambda: make_lif([simple('A'), simple('B')], order=['B', 'A']),
    'nesting': nesting,
    'types': types,
    'rgb': rgb,
    'xzxt': xzxt,
    'extra_axes': extra_axes,
    'all_unsupported': all_unsupported,
    'padded': padded,
    'bad_strides': bad_strides,
    'broken_refs': broken_refs,
    'truncated': truncated,
    'unicode': unicode_,
    'timestamps': timestamps,
    'dup_names': lambda: make_lif([simple('Same'), simple('Same')]),
    'trailer_garbage': lambda: make_lif([simple('Plain')], trailer=b'\0\0\0\0\xff\xff'),
    'lof': lof,
    'not_lif': lambda: b'A' * 64,
}


def main():
    here = Path(__file__).resolve().parent
    out = (Path(sys.argv[1]) if len(sys.argv) > 1
           else here.parent.parent / 'src' / 'tests' / 'lif' / 'data')
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in FIXTURES.items():
        (out / (name + '.lif')).write_bytes(fn())
    print('wrote %d fixtures to %s' % (len(FIXTURES), out))


if __name__ == '__main__':
    main()
