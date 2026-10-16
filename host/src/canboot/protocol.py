"""Wire format shared with ASW/APP_IF/PROTOCOL.md, protocol version 2."""
from dataclasses import dataclass
from enum import IntEnum
import struct

from .errors import ProtocolError

REQUEST_ID = 0x201
REPLY_ID = 0x200
DATA_ID = 0x203
PROTOCOL_VERSION = 2
WRITE_ALIGNMENT = 8
MAX_READ_WORDS = 128


class Command(IntEnum):
    ERASE = 0
    UPDATE_BEGIN = 1
    WRITE_BEGIN = 2
    WRITE_COMMIT = 3
    UPDATE_FINISH = 4
    APP_RUN = 5
    RESET = 6
    GET_INFO = 7
    WRITE_DATA = 8
    GET_STATUS = 9
    GET_MEMORY_MAP = 10
    KEEP_ALIVE = 11
    READ = 12
    ERASE_RANGE = 13
    VERIFY = 14
    UPDATE_ABORT = 15


class ReplyType(IntEnum):
    ACK = 0
    VERSIONS = 1
    SYSTEM_STATUS = 2
    CAPABILITIES = 3
    STATE = 4
    MEMORY_START = 5
    MEMORY_SIZE = 6
    READ_DATA = 7
    CRC = 8


class Status(IntEnum):
    IN_PROGRESS = 0
    COMPLETE = 1
    FAILED = 2
    RETRY = 3
    INVALID = 4


class Error(IntEnum):
    NONE = 0
    UPDATE_TIMEOUT = 1
    FLASH = 2
    BLOCK_CRC = 3
    DATA = 4
    BLOCK_CONFIG = 5
    APP_SIZE = 6
    APP_CRC = 7
    ADDRESS = 8
    ALIGNMENT = 9
    STATE = 10
    SEQUENCE = 11
    LENGTH = 12
    TX_TIMEOUT = 13


class State(IntEnum):
    STARTUP = 0
    MENU = 1
    UPDATE = 2
    APP_RUN = 3
    RESET = 4
    ERROR = 5


@dataclass(frozen=True)
class Frame:
    arbitration_id: int
    data: bytes
    is_extended_id: bool = False
    is_error_frame: bool = False
    is_remote_frame: bool = False
    is_fd: bool = False


@dataclass(frozen=True)
class Reply:
    kind: ReplyType
    tag: int
    payload: bytes

    @classmethod
    def decode(cls, data: bytes):
        if len(data) != 8:
            raise ProtocolError(f"Expected an 8-byte reply, received {len(data)} bytes")
        try:
            return cls(ReplyType(data[7]), data[6], bytes(data[:6]))
        except ValueError as exc:
            raise ProtocolError(f"Unknown reply type {data[7]}") from exc

    def acknowledgement(self):
        try:
            return (Command(self.payload[1]), Status(self.payload[0]), Error(self.payload[2]),
                    struct.unpack_from("<H", self.payload, 4)[0])
        except ValueError as exc:
            raise ProtocolError("Unknown command, status, or error in acknowledgement") from exc


def request(command: Command, tag: int, arguments: bytes = b"") -> Frame:
    if len(arguments) > 6 or not 1 <= tag <= 255:
        raise ValueError("Requests require at most six argument bytes and a tag from 1 to 255")
    return Frame(REQUEST_ID, arguments.ljust(6, b"\0") + bytes([tag, command]))


def write_data(tag: int, offset: int, data: bytes) -> Frame:
    if len(data) not in (2, 4) or not 0 <= offset <= 65535 or not 1 <= tag <= 255:
        raise ValueError("Data frames require one or two words, a valid offset, and a nonzero tag")
    return Frame(DATA_ID, struct.pack("<H", offset) + data.ljust(4, b"\xff") + bytes([tag, len(data) // 2]))


def require_reply(replies: list[Reply], kind: ReplyType) -> bytes:
    matches = [r.payload for r in replies if r.kind == kind]
    if not matches:
        raise ProtocolError(f"Missing {kind.name} reply")
    if any(payload != matches[0] for payload in matches):
        raise ProtocolError(f"Conflicting {kind.name} replies")
    return matches[0]


@dataclass(frozen=True)
class MemoryMap:
    start: int
    words: int
    sector_words: int
    block_words: int

    def validate(self):
        if (not 0 <= self.start <= 0xFFFFFFFF or self.words <= 0 or
                self.start + self.words > 0x100000000 or self.start % WRITE_ALIGNMENT or
                self.words % WRITE_ALIGNMENT or self.sector_words <= 0 or
                self.sector_words % WRITE_ALIGNMENT or not 8 <= self.block_words <= 128 or
                self.block_words % WRITE_ALIGNMENT):
            raise ProtocolError("Device reported an unsupported flash layout")
        return self

    def check_range(self, address: int, words: int):
        if words <= 0 or address < self.start or address + words > self.start + self.words:
            raise ValueError("Address and word count must stay within application flash")


@dataclass(frozen=True)
class DeviceInfo:
    bootloader_version: int
    app_version: int
    hardware_version: int
    protocol_version: int
    serial_number: int
    data_words: int
    app_crc: int
    app_valid: bool
    last_error: int


@dataclass(frozen=True)
class DeviceStatus:
    state: State
    block_ready: bool
    words_written: int
    app_crc: int
    app_valid: bool
    last_error: int
