import importlib.util,struct,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('d81',Path(__file__).resolve().parents[1]/'tools/d81.py')
d81=importlib.util.module_from_spec(spec); spec.loader.exec_module(d81)
class D81Tests(unittest.TestCase):
    def test_reserved_volume_and_files(self):
        pc=bytes((i//512+i%512)&255 for i in range(737280))
        files=[('FD64',b'\x01\x08'+b'p'*777,2),('GLABIOS.ROM',b'r'*8192,1)]
        image=d81.create(pc,files)
        self.assertEqual(len(image),819200); self.assertEqual(d81.extract_pc(image),pc)
        for lba in range(1440):
            for half in range(2):
                t,s=d81.pc_location(lba,half)
                self.assertNotEqual(t,40); self.assertLessEqual(t,73)
                bam=d81.offset(40,1 if t<=40 else 2)+16+((t-1)%40)*6
                self.assertFalse(image[bam+1+s//8]&(1<<(s%8)))
        for n,(name,expected,kind) in enumerate(files):
            entry=d81.offset(40,3)+32*n; t,s=image[entry+3:entry+5]; content=bytearray(); seen=set()
            self.assertEqual(image[entry+2],0x80|kind)
            while t:
                self.assertGreaterEqual(t,74); self.assertNotIn((t,s),seen); seen.add((t,s))
                p=d81.offset(t,s); nt,ns=image[p:p+2]
                content+=image[p+2:p+256] if nt else image[p+2:p+1+ns]
                t,s=nt,ns
            self.assertEqual(content,expected)
            self.assertEqual(struct.unpack_from('<H',image,entry+30)[0],len(seen))
        for t in range(1,81):
            bam=d81.offset(40,1 if t<=40 else 2)+16+((t-1)%40)*6
            self.assertEqual(image[bam],sum(b.bit_count() for b in image[bam+1:bam+6]))
    def test_mapping_and_bounds(self):
        self.assertEqual(d81.pc_location(779,1),(39,39))
        self.assertEqual(d81.pc_location(780),(41,0))
        self.assertEqual(d81.pc_location(1439,1),(73,39))
        for lba in (-1,1440):
            with self.assertRaises(ValueError): d81.pc_location(lba)
        with self.assertRaises(ValueError): d81.create(b'',[])
        with self.assertRaises(ValueError): d81.create(bytes(737280),[('TOO BIG',bytes(80000),2)])
if __name__=='__main__': unittest.main()
