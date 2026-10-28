import csv
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import capture_csv as capture


class FakeSerial:
    def __init__(self, data):
        self.data = iter(data)
        self.closed = False

    def read(self, size):
        try:
            return bytes([next(self.data)])
        except StopIteration:
            raise KeyboardInterrupt

    def open(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.closed = True


class CaptureTests(unittest.TestCase):
    def test_raw_rows_timing_and_reset(self):
        stream = FakeSerial(
            b'partial startup\nboot\n'
            b'1000,-12.34,-1.234,-50.0,-6.0\r\n'
            b'1000,12.00,0.001,0.0,25.0\n'
            b'# debug\n1123,12.00,0.001,0.0,25.0\n'
            b'5,12.00,0.001,0.0,25.0\n')
        output = io.StringIO()
        stats = capture.CaptureStats()
        with self.assertRaisesRegex(capture.DeviceResetError, '1123 -> 5'):
            capture.capture(stream, output, ('42.5', 'load, warm', 'run-1'), stats)
        rows = list(csv.DictReader(io.StringIO(output.getvalue())))
        self.assertEqual(len(rows), 3)
        self.assertEqual([r['elapsed_s'] for r in rows], ['0.000', '0.000', '0.123'])
        self.assertEqual([r['interval_ms'] for r in rows], ['', '0', '123'])
        self.assertEqual(rows[0]['current_A'], '-1.234')
        self.assertEqual(rows[0]['rpm'], '-50.0')
        self.assertTrue(all(r['declared_duty_percent'] == '42.5' for r in rows))
        self.assertEqual(rows[0]['test_condition'], 'load, warm')
        self.assertEqual(stats.rows, 3)
        self.assertEqual(stats.duration_s, 0.123)

    def test_prompt_validation(self):
        with patch('builtins.input', side_effect=['nan', '-1', '101', 'abc', '0', '', 'test', '', 'id']), patch('sys.stdout', io.StringIO()):
            self.assertEqual(capture.prompt_metadata(), ('0', 'test', 'id'))
        with patch('builtins.input', side_effect=['100', 'test', 'id']):
            self.assertEqual(capture.prompt_metadata(), ('100', 'test', 'id'))

    def test_ctrl_c_closes_file_and_port_and_summarizes(self):
        for data, expected_rows in ((b'boot\n1000,12.00,0.001,0.0,25.0\n', 1), (b'boot\n', 0)):
            with self.subTest(rows=expected_rows), tempfile.TemporaryDirectory() as folder:
                stream = FakeSerial(data)
                console = io.StringIO()
                with patch('serial.Serial', return_value=stream), patch.object(capture, 'prompt_metadata', return_value=('25', 'test', 'id')), patch('sys.argv', ['capture_csv.py', '--port', 'COM6', '--output-dir', folder]), patch('sys.stdout', console):
                    self.assertEqual(capture.main(), 0)
                self.assertTrue(stream.closed)
                path = next(Path(folder).glob('*.csv'))
                with path.open(newline='') as saved:
                    self.assertEqual(len(list(csv.DictReader(saved))), expected_rows)
                self.assertIn(str(path), console.getvalue())
                self.assertIn(f'Data rows saved: {expected_rows}', console.getvalue())
                self.assertIn('Recorded duration', console.getvalue())
                if expected_rows == 0:
                    self.assertIn('No valid CSV', console.getvalue())
                # Windows permits this only after the writer has closed the file.
                path.unlink()


if __name__ == '__main__':
    unittest.main()
