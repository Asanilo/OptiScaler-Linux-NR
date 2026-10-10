import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np

spec = importlib.util.spec_from_file_location('capture_analysis', Path(__file__).parents[1] / 'tools/analyze_nr_capture.py')
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


class CaptureAnalysis(unittest.TestCase):
    def image(self, root, frame, stage, values, fmt=2, colour='linear_hdr'):
        values = np.asarray(values)
        h, w, _ = values.shape
        pitch, offset = 256, 512
        raw = bytearray(offset + pitch * h)
        for y in range(h):
            row = values[y].tobytes()
            raw[offset + y * pitch:offset + y * pitch + len(row)] = row
        name = f'{frame}-{stage}.raw'
        (root / name).write_bytes(raw)
        return dict(frame=frame, stage=stage, file=name, status='ok', colour_space=colour,
                    width=w, height=h, format=fmt, row_pitch=pitch, offset=offset, bytes=len(raw))

    def test_hdr_padding_negative_and_dark(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = np.array([[[0, 0, 0, 1], [-.5, 8, 1, .25]], [[.1, .2, .3, 1], [4, 5, 6, 0]]], '<f4')
            item = self.image(root, 0, 'original', a)
            np.testing.assert_array_equal(analysis.decode(root, item), a)
            (root / item['file']).write_bytes(b'bad')
            with self.assertRaises(ValueError):
                analysis.decode(root, item)

    def test_half_srgb_bgra_and_packed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = np.array([[[.5, 2, 0, 1]]], '<f2')
            item = self.image(root, 0, 'proxy', a, 10, 'srgb_encoded')
            np.testing.assert_array_equal(analysis.decode(root, item), a)
            self.assertAlmostEqual(float(analysis.linear(a.astype('f4'), 'srgb_encoded')[0, 0, 0]), .214041, places=5)
            item = self.image(root, 0, 'bgra', np.array([[[10, 20, 30, 255]]], 'u1'), 87)
            np.testing.assert_allclose(analysis.decode(root, item)[0, 0], [30/255, 20/255, 10/255, 1])
            packed = np.array([[[1 | (511 << 10) | (1023 << 20) | (3 << 30)]]], '<u4')
            item = self.image(root, 0, 'packed', packed, 24)
            np.testing.assert_allclose(analysis.decode(root, item)[0, 0], [1/1023, 511/1023, 1, 1])

    def test_known_model_flicker_and_roi(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            images = []
            for frame in range(3):
                for stage in ('original', 'proxy', 'model_raw', 'resolve'):
                    a = np.full((4, 4, 4), .25, '<f4')
                    if stage in ('model_raw', 'resolve') and frame == 1:
                        a[:2, :2, :3] = .75
                    images.append(self.image(root, frame, stage, a))
            (root / 'manifest.json').write_text(json.dumps(dict(schema_version=2, status='complete',
                frames=[dict(nr_frame=i, whitepoint=1) for i in range(3)], images=images)))
            result = analysis.analyze(root, root / 'analysis', {'light': (0, 0, .5, .5), 'control': (.5, .5, .5, .5)})
            rows = [r for r in result['metrics'] if r['frame'] == 1]
            for r in rows:
                expected = .5 if r['roi'] == 'light' and r['stage'] in ('model_raw', 'resolve', 'model_raw_edit', 'resolve_edit') else 0
                self.assertAlmostEqual(r['temporal_mae'], expected)
            self.assertTrue((root / 'analysis/00-original.png').read_bytes().startswith(b'\x89PNG'))
            with self.assertRaises(FileExistsError):
                analysis.analyze(root, root / 'analysis', {})

    def test_roi_validation(self):
        for invalid in ('x:0,0,2,1', 'x:0,0,0,1', 'x:nan,0,1,1'):
            with self.assertRaises(ValueError):
                analysis.parse_roi(invalid)


if __name__ == '__main__':
    unittest.main()
