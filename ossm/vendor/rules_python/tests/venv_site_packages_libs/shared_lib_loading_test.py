import importlib.util
import os
import unittest

from elftools.elf.elffile import ELFFile
from macholib import mach_o
from macholib.MachO import MachO

ELF_MAGIC = b"\x7fELF"
MACHO_MAGICS = (
    b"\xce\xfa\xed\xfe",  # 32-bit big-endian
    b"\xcf\xfa\xed\xfe",  # 64-bit big-endian
    b"\xfe\xed\xfa\xce",  # 32-bit little-endian
    b"\xfe\xed\xfa\xcf",  # 64-bit little-endian
)


class SharedLibLoadingTest(unittest.TestCase):
    def test_shared_library_linking(self):
        try:
            import ext_with_libs.adder
        except ImportError as e:
            spec = importlib.util.find_spec("ext_with_libs.adder")
            if not spec or not spec.origin:
                self.fail(f"Import failed and could not find module spec: {e}")

            info = self._get_linking_info(spec.origin)

            # Give a useful error message for debugging.
            self.fail(
                f"Failed to import adder extension.\n"
                f"Original error: {e}\n"
                f"Linking info for {spec.origin}:\n"
                f"  RPATHs: {info.get('rpaths', 'N/A')}\n"
                f"  Needed libs: {info.get('needed', 'N/A')}"
            )

        # Check that the module was loaded from the venv.
        self.assertIn(".venv/", ext_with_libs.adder.__file__)

        adder_path = os.path.realpath(ext_with_libs.adder.__file__)

        with open(adder_path, "rb") as f:
            magic_bytes = f.read(4)

        if magic_bytes == ELF_MAGIC:
            self._assert_elf_linking(adder_path)
        elif magic_bytes in MACHO_MAGICS:
            self._assert_macho_linking(adder_path)
        else:
            self.fail(f"Unsupported file format for adder: magic bytes {magic_bytes!r}")

        # Check the function works regardless of format.
        self.assertEqual(ext_with_libs.adder.do_add(), 2)

    def _get_linking_info(self, path):
        """Parses a shared library and returns its rpaths and dependencies."""
        path = os.path.realpath(path)
        with open(path, "rb") as f:
            magic_bytes = f.read(4)

        if magic_bytes == ELF_MAGIC:
            return self._get_elf_info(path)
        elif magic_bytes in MACHO_MAGICS:
            return self._get_macho_info(path)
        return {}

    def _get_elf_info(self, path):
        """Extracts linking information from an ELF file."""
        info = {"rpaths": [], "needed": [], "undefined_symbols": []}
        with open(path, "rb") as f:
            elf = ELFFile(f)
            dynamic = elf.get_section_by_name(".dynamic")
            if dynamic:
                for tag in dynamic.iter_tags():
                    if tag.entry.d_tag == "DT_NEEDED":
                        info["needed"].append(tag.needed)
                    elif tag.entry.d_tag == "DT_RPATH":
                        info["rpaths"].append(tag.rpath)
                    elif tag.entry.d_tag == "DT_RUNPATH":
                        info["rpaths"].append(tag.runpath)

            dynsym = elf.get_section_by_name(".dynsym")
            if dynsym:
                info["undefined_symbols"] = [
                    s.name
                    for s in dynsym.iter_symbols()
                    if s.entry["st_shndx"] == "SHN_UNDEF"
                ]
        return info

    def _get_macho_info(self, path):
        """Extracts linking information from a Mach-O file."""
        info = {"rpaths": [], "needed": []}
        macho = MachO(path)
        for header in macho.headers:
            for cmd_load, cmd, data in header.commands:
                if cmd_load.cmd == mach_o.LC_LOAD_DYLIB:
                    info["needed"].append(data.decode().strip("\x00"))
                elif cmd_load.cmd == mach_o.LC_RPATH:
                    info["rpaths"].append(data.decode().strip("\x00"))
        return info

    def _assert_elf_linking(self, path):
        """Asserts dynamic linking properties for an ELF file."""
        info = self._get_elf_info(path)
        self.assertIn("libincrement.so", info["needed"])
        self.assertIn("increment", info["undefined_symbols"])

    def _assert_macho_linking(self, path):
        """Asserts dynamic linking properties for a Mach-O file."""
        info = self._get_macho_info(path)
        self.assertIn("@rpath/libincrement.dylib", info["needed"])


if __name__ == "__main__":
    unittest.main()
