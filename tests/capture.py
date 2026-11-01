"""Real-engine publication checks; stdlib only, writes exclusively to temp dirs."""
import base64
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("capture_server", ROOT / "tools/capture_server.py")
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


def picture(pose="stand"):
    w, h = 640, 480
    pixels = bytearray([255, 255, 255] * w * h)
    def box(x0, y0, x1, y1, color):
        for y in range(y0,y1):
            for x in range(x0,x1): pixels[(y*w+x)*3:(y*w+x)*3+3] = bytes(color)
    red=(185,55,45)
    box(260,180,365,350,red);box(275,100,380,205,red)
    box(280,340,310,430,red);box(335,340,365,430,red)
    box(355,210,410 if pose!='punch2' else 490,245,red)
    if pose=='kick': box(355,300,470,335,red)
    box(352,128,365,142,(255,255,255))
    raw=b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
    def chunk(t,b): return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
    data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
    return 'data:image/png;base64,'+base64.b64encode(data).decode()


class Capture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.request={"name":"TESTCLAY","mode":"tripod","frames":{key:{"image":picture(key)} for key in capture.IDS}}

    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='clay-capture-')
        self.root=Path(self.tmp.name)
        self.store=capture.CaptureStore(self.root/'creatures',ROOT/'data/primrage',ROOT/'build/clayfight')

    def tearDown(self): self.tmp.cleanup()

    def test_png_checks_pixels_crc_and_truncation(self):
        data=capture.png(self.request['frames']['stand']['image'])
        self.assertTrue(data.startswith(b'\x89PNG'))
        for broken in [data[:-15],data[:40]+b'wrong'+data[45:]]:
            with self.assertRaises(ValueError): capture.png('data:image/png;base64,'+base64.b64encode(broken).decode())

    def test_missing_portrait_and_path_names_are_rejected(self):
        request=copy.deepcopy(self.request);del request['frames']['portrait']
        with self.assertRaises(ValueError): self.store.publish(request)
        request=copy.deepcopy(self.request);request['name']='../ESCAPE'
        with self.assertRaises(ValueError): self.store.publish(request)
        self.assertFalse(self.store.creatures.exists())

    def test_real_engine_publication_and_match(self):
        request=copy.deepcopy(self.request)
        request['frames']['stand']['source']=request['frames']['stand']['image']
        result=self.store.publish(request)
        folder=self.store.creatures/result['folder']
        self.assertTrue((folder/'preview-sheet.bmp').is_file())
        self.assertTrue((folder/'sources/stand.png').is_file())
        self.assertEqual(len(list((folder/'photos').glob('*.png'))),16)
        self.assertIn('portrait photos/portrait.png',(folder/'fighter.txt').read_text())
        self.assertEqual(json.loads((folder/'capture.json').read_text())['poses'],capture.IDS)
        run=subprocess.run([str(ROOT/'build/clayfight'),'-creatures',str(self.store.creatures),'-fight','testclay','sauron',
             '-cpu','-shots',str(self.root/'fight'),'-tics','1200','-every','1199'],cwd=ROOT,capture_output=True,text=True,timeout=60)
        self.assertEqual(run.returncode,0,run.stderr)
        self.assertTrue((self.root/'fight01199.bmp').is_file())
        self.assertIn('W_PhotoFinish: portrait',run.stdout)
        with self.assertRaisesRegex(ValueError,'имя'):self.store.publish(request)
        self.assertEqual(len(list(self.store.creatures.glob('custom-*'))),1)
        self.assertEqual(list(self.store.creatures.glob('.capture-*')),[])

    def test_failed_engine_does_not_publish_half_a_creature(self):
        self.store.executable=Path('/usr/bin/false')
        with self.assertRaisesRegex(ValueError,'собрать'): self.store.publish(self.request)
        self.assertEqual(list(self.store.creatures.iterdir()),[])

if __name__=='__main__':unittest.main()
