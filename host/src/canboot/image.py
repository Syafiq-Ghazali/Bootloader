"""Word-addressed binary images and erased-fill CRC calculation."""
from dataclasses import dataclass
from pathlib import Path
import zlib

from .protocol import MemoryMap, WRITE_ALIGNMENT


@dataclass(frozen=True)
class FirmwareImage:
    """Segments contain raw little-endian word bytes at target word addresses."""
    segments: tuple[tuple[int, bytes], ...]

    @classmethod
    def from_binary(cls, data: bytes, address: int):
        if not data:
            raise ValueError("Firmware binary is empty")
        if data.startswith(b"\x7fELF"):
            raise ValueError("ELF/.out files must be exported as a raw little-endian .bin first")
        return cls(((address, bytes(data)),))

    @classmethod
    def from_file(cls, path: str | Path, address: int):
        path = Path(path)
        if path.suffix.lower() != ".bin":
            raise ValueError("Use a raw little-endian .bin; .out and TI/Intel .hex are not parsed")
        return cls.from_binary(path.read_bytes(), address)

    def prepare(self, memory: MemoryMap):
        memory.validate()
        if not self.segments:
            raise ValueError("Firmware image has no segments")
        region = bytearray(b"\xff\xff" * memory.words)
        previous_end = memory.start
        span_words = 0
        for address, data in sorted(self.segments, key=lambda segment: segment[0]):
            if not data:
                raise ValueError("Firmware segment is empty")
            words = (len(data) + 1) // 2
            memory.check_range(address, words)
            if address < previous_end:
                raise ValueError("Firmware segments overlap")
            offset = (address - memory.start) * 2
            region[offset:offset + len(data)] = data
            previous_end = address + words
            span_words = previous_end - memory.start
        if region[:2] == b"\xff\xff":
            raise ValueError("Firmware must include an application entry word at the flash start")
        padded_words = ((span_words + WRITE_ALIGNMENT - 1) // WRITE_ALIGNMENT) * WRITE_ALIGNMENT
        return PreparedImage(bytes(region), span_words, padded_words, zlib.crc32(region))


@dataclass(frozen=True)
class PreparedImage:
    region: bytes
    image_words: int
    padded_words: int
    crc: int

    def blocks(self, memory: MemoryMap):
        for offset in range(0, self.padded_words, memory.block_words):
            data = self.region[offset * 2:min(offset + memory.block_words, self.padded_words) * 2]
            if data != b"\xff" * len(data):
                yield memory.start + offset, data
