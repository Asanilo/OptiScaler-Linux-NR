#!/usr/bin/env python3
"""Decode NR capture v2 without losing HDR values or padded-row information.

ROIs use normalized x,y,width,height coordinates shared across stage resolutions.
Metrics describe the captured window; they do not establish a cause or prove that
longer gameplay is flicker-free. Previews use one fixed white point per batch.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import struct
import zlib

import numpy as np

# DXGI formats, little-endian storage, including RG/R guide sidecars.
FORMATS = {2: ('<f4', 4), 10: ('<f2', 4), 16: ('<f4', 2), 34: ('<f2', 2),
           41: ('<f4', 1), 54: ('<f2', 1), 28: ('u1', 4), 29: ('u1', 4),
           87: ('u1', 4), 91: ('u1', 4), 24: ('<u4', 1), 26: ('<u4', 1),
           35: ('<u2', 2), 37: ('<i2', 2), 40: ('<f4', 1), 56: ('<u2', 1)}


def decode(root, image):
    if image['status'] != 'ok':
        raise ValueError('Image not completed: ' + image['status'])
    path = (root / image['file']).resolve()
    if path.parent != root.resolve():
        raise ValueError('Image path escapes capture directory')
    dtype, channels = FORMATS[image['format']]
    dtype = np.dtype(dtype)
    w, h = image['width'], image['height']
    pitch, offset = image['row_pitch'], image['offset']
    if min(w, h) <= 0 or pitch < w * channels * dtype.itemsize or offset < 0:
        raise ValueError('Invalid footprint')
    raw = path.read_bytes()
    if len(raw) != image['bytes'] or len(raw) < offset + (h - 1) * pitch + w * channels * dtype.itemsize:
        raise ValueError('Truncated or inconsistent raw image')
    a = np.ndarray((h, w, channels), dtype=dtype, buffer=raw, offset=offset,
                   strides=(pitch, channels * dtype.itemsize, dtype.itemsize)).astype(np.float32)
    if image['format'] == 26:
        # DXGI R11G11B10_FLOAT: unsigned 5-bit exponents, fractions 6/6/5.
        # Shift each channel into IEEE half's exponent/fraction positions.
        # This also preserves subnormals, infinities and NaNs exactly.
        # https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format
        packed = np.ndarray((h, w), dtype='<u4', buffer=raw, offset=offset, strides=(pitch, 4))
        components = []
        for shift, bits in ((0, 11), (11, 11), (22, 10)):
            half = (((packed >> shift) & ((1 << bits) - 1)) << (15 - bits)).astype('<u2')
            components.append(half.view('<f2').astype(np.float32))
        a = np.stack(components, axis=-1)
    elif image['format'] in (35, 56):
        a /= 65535
    elif image['format'] == 37:
        a = np.maximum(a / 32767, -1)
    elif image['format'] == 24:
        # Re-read as integers: float32 would lose low packed bits.
        packed = np.ndarray((h, w), dtype='<u4', buffer=raw, offset=offset, strides=(pitch, 4))
        a = np.stack([(packed >> shift) & mask for shift, mask in
                      ((0, 1023), (10, 1023), (20, 1023), (30, 3))], axis=-1).astype(np.float32)
        a /= np.array([1023, 1023, 1023, 3], dtype=np.float32)
    elif dtype == np.dtype('u1'):
        a /= 255
        if image['format'] in (87, 91):
            a = a[..., [2, 1, 0, 3]]
    return a


def linear(a, colour):
    rgb = a[..., :3]
    if colour == 'srgb_encoded':
        return np.where(rgb <= .04045, rgb / 12.92, ((np.maximum(rgb, 0) + .055) / 1.055) ** 2.4)
    return rgb


def png(path, rgb):
    """Lossless preview only; statistics always use unquantized source floats."""
    rgb = np.asarray(np.clip(rgb, 0, 1) * 255 + .5, dtype=np.uint8)
    h, w, _ = rgb.shape
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    scanlines = b''.join(b'\0' + row.tobytes() for row in rgb)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))


def region(a, roi):
    x, y, w, h = roi
    height, width = a.shape[:2]
    x0, y0 = int(x * width), int(y * height)
    x1, y1 = min(width, int(np.ceil((x + w) * width))), min(height, int(np.ceil((y + h) * height)))
    return a[y0:y1, x0:x1]


def analyze(root, output, rois):
    manifest = json.loads((root / 'manifest.json').read_text())
    if manifest['schema_version'] != 2:
        raise ValueError('Expected schema_version 2; legacy before/after captures are ambiguous')
    output.mkdir(parents=True, exist_ok=False)
    rows, previous, frames = [], {}, manifest['frames']
    whites = [f.get('whitepoint', 1) for f in frames]
    fixed_white = next((w for w in whites if w is not None and w > 0), 1)
    warnings = []
    if manifest['status'] != 'complete':
        warnings.append('Incomplete capture: ' + manifest['status'])
    digests = {}
    for item in manifest['images']:
        if item['status'] != 'ok':
            warnings.append('Skipped ' + item['file'] + ': ' + item['status'])
            continue
        a = decode(root, item)
        digests[item['file']] = hashlib.sha256((root / item['file']).read_bytes()).hexdigest()
        colour, stage, frame = item['colour_space'], item['stage'], item['frame']
        if a.shape[-1] < 3:
            continue  # Guides decoded/validated; not RGB luminance.
        rgb = linear(a, colour)
        if 'unknown' in colour:
            warnings.append(stage + ': transfer unknown; metrics are game signal values, not physical luminance')
        measures = [(stage, rgb)]
        partner_stage = {'model_raw': 'proxy', 'resolve': 'original', 'final_filtered': 'resolve',
                         'final_cached': 'original'}.get(stage)
        partner = next((i for i in manifest['images'] if i['frame'] == frame and
                        i['stage'] == partner_stage and i['status'] == 'ok'), None)
        if partner:
            partner_rgb = linear(decode(root, partner), partner['colour_space'])
            if partner_rgb.shape == rgb.shape and partner['colour_space'] == colour:
                measures.append((stage + '_edit', rgb - partner_rgb))
            else:
                warnings.append(stage + ': paired edit skipped due to dimensions or colour-space mismatch')
        for metric_stage, measured_rgb in measures:
            for name, roi in rois.items():
                crop = region(measured_rgb, roi)
                if not np.isfinite(crop).all():
                    warnings.append(f'{frame}/{stage}/{name}: nonfinite pixels; metrics omitted')
                    continue
                luma = crop @ np.array([.2126, .7152, .0722], dtype=np.float32)
                key = metric_stage, name
                prev = previous.get(key)
                consecutive = prev is not None and prev[0] + 1 == frame and prev[1].shape == luma.shape
                diff = np.abs(luma - prev[1]) if consecutive else None
                rows.append({'frame': frame, 'nr_frame': frames[frame].get('nr_frame'), 'stage': metric_stage,
                             'roi': name, 'units': 'linear_display' if colour == 'srgb_encoded' else colour,
                             'mean': float(luma.mean()), 'p95': float(np.quantile(luma, .95)),
                             'max': float(luma.max()), 'temporal_mae': float(diff.mean()) if diff is not None else None,
                             'temporal_p95': float(np.quantile(diff, .95)) if diff is not None else None})
                previous[key] = frame, luma.copy()
        # Uniform display transform across frames; never auto-normalize each frame.
        preview = a[..., :3] if colour != 'linear_hdr' else np.maximum(rgb, 0) / (fixed_white + np.maximum(rgb, 0))
        if colour == 'linear_hdr':
            preview = np.where(preview <= .0031308, preview * 12.92, 1.055 * preview ** (1 / 2.4) - .055)
        png(output / f'{frame:02}-{stage}.png', np.nan_to_num(preview))
    with (output / 'metrics.csv').open('w', newline='') as file:
        fields = ['frame', 'nr_frame', 'stage', 'roi', 'units', 'mean', 'p95', 'max', 'temporal_mae', 'temporal_p95']
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    result = {'capture': str(root.resolve()), 'capture_status': manifest['status'], 'rois': rois,
              'preview_fixed_white': fixed_white, 'frames': frames, 'metrics': rows,
              'raw_sha256': digests, 'warnings': sorted(set(warnings)),
              'limits': 'No automatic root-cause verdict. Stage colour spaces differ. Eight NR recordings may miss the event; camera motion is not registered. Cached frames omit unevaluated stages.'}
    (output / 'analysis.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    return result


def parse_roi(value):
    name, coords = value.split(':', 1)
    roi = tuple(float(v) for v in coords.split(','))
    if len(roi) != 4 or not name or not all(np.isfinite(roi)):
        raise ValueError('ROI must be name:x,y,width,height')
    x, y, w, h = roi
    if min(x, y) < 0 or min(w, h) <= 0 or x + w > 1 or y + h > 1:
        raise ValueError('ROI coordinates must fit inside normalized [0,1] image')
    return name, roi


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True, help='new output directory')
    parser.add_argument('--roi', action='append', default=[], help='light:0.1,0.2,0.15,0.2')
    args = parser.parse_args()
    rois = dict(parse_roi(v) for v in args.roi) or {'full_frame': (0, 0, 1, 1)}
    result = analyze(args.capture, args.output, rois)
    print(json.dumps({'frames': len(result['frames']), 'warnings': result['warnings'], 'output': str(args.output)}))


if __name__ == '__main__':
    main()
