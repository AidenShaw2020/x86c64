import importlib.util
from pathlib import Path
import tempfile
import struct
import unittest
spec=importlib.util.spec_from_file_location("images",Path(__file__).resolve().parents[1]/"tools/images.py")
images=importlib.util.module_from_spec(spec); spec.loader.exec_module(images)
TEST_TEMP=Path(__file__).resolve().parents[1]/"build"/"test-temp"
TEST_TEMP.mkdir(parents=True,exist_ok=True)
class ImageTests(unittest.TestCase):
    def test_layout(self):
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as d:
            p=Path(d)/"disk.img"; images.create_disk(p)
            self.assertEqual(p.stat().st_size,32*1024*1024)
            with p.open("rb") as f:
                m=f.read(512); f.seek(images.START*512); b=f.read(512)
                self.assertEqual(m[510:],b"\x55\xaa"); self.assertEqual(b[510:],b"\x55\xaa")
                self.assertEqual(struct.unpack_from("<II",m,454),(64,65472))
                self.assertEqual(b[54:62],b"FAT16   ")
                spf=struct.unpack_from("<H",b,22)[0]
                clusters=(65472-1-2*spf-32)//4
                self.assertTrue(4085<=clusters<65525)
                self.assertGreaterEqual(spf*512,(clusters+2)*2)
                f.seek(65*512); first=f.read(spf*512)
                f.seek((65+spf)*512); second=f.read(spf*512)
                self.assertEqual(first,second); self.assertEqual(first[:4],b"\xf8\xff\xff\xff")
    def test_refuse_overwrite(self):
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as d:
            p=Path(d)/"existing"; p.write_bytes(b"keep")
            for function in (images.create_disk,images.create_ram):
                with self.assertRaises(FileExistsError): function(p)
                self.assertEqual(p.read_bytes(),b"keep")
    def test_ram_size_and_zero(self):
        with tempfile.TemporaryDirectory(dir=TEST_TEMP) as d:
            p=Path(d)/"ram"; images.create_ram(p)
            self.assertEqual(p.stat().st_size,1048576)
            self.assertEqual(set(p.read_bytes()),{0})
if __name__=="__main__": unittest.main()
