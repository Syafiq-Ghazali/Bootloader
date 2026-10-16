"""Synchronous bootloader API with serialized keep-alives and CAN requests."""
import logging
import math
import struct
import threading
import time
from typing import Callable
import zlib

from .errors import (BootloaderError, CommandError, ProtocolError, ReplyTimeout,
                     VerificationError)
from .image import FirmwareImage
from .protocol import (Command, DeviceInfo, DeviceStatus, Error, Frame, MAX_READ_WORDS,
                       MemoryMap, PROTOCOL_VERSION, REPLY_ID, Reply, ReplyType, State,
                       Status, WRITE_ALIGNMENT, request, require_reply, write_data)
from .transport import PythonCanTransport, Transport

LOG = logging.getLogger(__name__)


class BootloaderClient:
    def __init__(self, transport: Transport, *, timeout: float = 0.2,
                 operation_timeout: float = 30.0, retries: int = 2,
                 auto_keep_alive: bool = True, keep_alive_interval: float = 0.25):
        if any(not math.isfinite(value) or value <= 0 for value in
               (timeout, operation_timeout, keep_alive_interval)):
            raise ValueError("Timeouts and keep-alive interval must be positive and finite")
        if not 0 <= retries <= 10 or keep_alive_interval >= 1:
            raise ValueError("Use 0-10 retries and a keep-alive interval below one second")
        self.transport = transport
        self.timeout = timeout
        self.operation_timeout = operation_timeout
        self.retries = retries
        self.auto_keep_alive = auto_keep_alive
        self.keep_alive_interval = keep_alive_interval
        self.memory: MemoryMap | None = None
        self.device: DeviceInfo | None = None
        self._lock = threading.RLock()
        self._stop = threading.Event()
        self._worker: threading.Thread | None = None
        self._background_error: Exception | None = None
        self._last_activity = time.monotonic()
        self._tag = 0
        self._last_block_tag = 0
        self._update_active = False
        self._terminal = False
        self._closed = False

    @classmethod
    def open_pcan(cls, *, channel="PCAN_USBBUS1", bitrate=500000, device_id=None, **options):
        transport = PythonCanTransport.pcan(channel, bitrate, device_id)
        try:
            return cls(transport, **options)
        except Exception:
            transport.close()
            raise

    def __enter__(self):
        try:
            self.connect()
            return self
        except BaseException:
            self.close()
            raise

    def __exit__(self, exc_type, exc, traceback):
        try:
            self.close()
        except BootloaderError:
            if exc is None:
                raise
            LOG.exception("CAN shutdown failed while handling another error")

    def connect(self):
        with self._lock:
            self.device = self.get_info()
            if self.device.protocol_version != PROTOCOL_VERSION or self.device.data_words != 2:
                raise ProtocolError(f"Expected protocol {PROTOCOL_VERSION} with two-word data frames; "
                                    f"device reports protocol {self.device.protocol_version}")
            self.memory = self.get_memory_map()
            if self.auto_keep_alive and self._worker is None:
                self._worker = threading.Thread(target=self._keep_alive_loop, name="canboot-keep-alive", daemon=True)
                self._worker.start()
            return self.device

    def close(self):
        self._stop.set()
        if self._worker is not None and self._worker is not threading.current_thread():
            self._worker.join()
        with self._lock:
            if not self._closed:
                self._closed = True
                self.transport.close()

    def _check_health(self):
        if self._closed:
            raise BootloaderError("Client is closed")
        if self._terminal:
            raise BootloaderError("Device was reset or started; reconnect with a new client")
        if self._background_error is not None:
            raise BootloaderError(f"Keep-alive failed: {self._background_error}") from self._background_error

    def _keep_alive_loop(self):
        while not self._stop.wait(min(self.keep_alive_interval, 0.05)):
            if not self._lock.acquire(timeout=0.05):
                continue
            try:
                if self._terminal:
                    return
                if time.monotonic() - self._last_activity >= self.keep_alive_interval:
                    self.keep_alive()
            except Exception as exc:
                self._background_error = exc
                return
            finally:
                self._lock.release()

    def _new_tag(self):
        self._tag = self._tag % 255 + 1
        if self._tag == self._last_block_tag:
            self._tag = self._tag % 255 + 1
        return self._tag

    def _exchange(self, command: Command, frame: Frame, *, timeout=None, retries=0,
                  expected_offset=None):
        """Only repeat a request when its caller explicitly permits retransmission."""
        self._check_health()
        timeout = self.timeout if timeout is None else timeout
        tag = frame.data[6]
        replies: list[Reply] = []
        for attempt in range(retries + 1):
            self.transport.send(frame, self.timeout)
            deadline = time.monotonic() + timeout
            while (remaining := deadline - time.monotonic()) > 0:
                received = self.transport.receive(remaining)
                if received is None:
                    break
                if (received.arbitration_id != REPLY_ID or received.is_extended_id or
                        received.is_error_frame or received.is_remote_frame or received.is_fd):
                    continue
                if len(received.data) < 7 or received.data[6] != tag:
                    continue
                reply = Reply.decode(received.data)
                if reply.kind != ReplyType.ACK:
                    replies.append(reply)
                    continue
                ack_command, status, error, offset = reply.acknowledgement()
                if ack_command != command:
                    continue
                if status == Status.IN_PROGRESS:
                    continue
                if status != Status.COMPLETE:
                    raise CommandError(command, status, error, offset)
                if error != Error.NONE:
                    raise ProtocolError("Successful acknowledgement contains an error")
                if expected_offset is not None and offset != expected_offset:
                    # All DATA frames use the block tag. A delayed prior ACK is
                    # distinguishable only by its next-word offset.
                    if offset < expected_offset:
                        continue
                    raise ProtocolError(f"Device expects word {offset}, expected {expected_offset}")
                self._last_activity = time.monotonic()
                return replies
            if attempt < retries:
                LOG.debug("Retrying %s, tag %d (%d/%d)", command.name, tag, attempt + 1, retries)
        raise ReplyTimeout(f"No completion for {command.name} (tag {tag}); "
                           "query device status before repeating an erase, finish, run, or reset")

    def _request(self, command, arguments=b"", *, tag=None, long=False, retry=False):
        with self._lock:
            return self._exchange(command, request(command, self._new_tag() if tag is None else tag, arguments),
                                  timeout=self.operation_timeout if long else self.timeout,
                                  retries=self.retries if retry else 0)

    def get_info(self):
        replies = self._request(Command.GET_INFO, retry=True)
        versions = struct.unpack("<HHH", require_reply(replies, ReplyType.VERSIONS))
        capabilities = struct.unpack("<HHH", require_reply(replies, ReplyType.CAPABILITIES))
        crc, valid, error = struct.unpack("<IBB", require_reply(replies, ReplyType.SYSTEM_STATUS))
        self.device = DeviceInfo(*versions, *capabilities, crc, bool(valid), error)
        return self.device

    def get_status(self):
        with self._lock:
            replies = self._request(Command.GET_STATUS, retry=True)
            state, ready, written = struct.unpack("<BBI", require_reply(replies, ReplyType.STATE))
            crc, valid, error = struct.unpack("<IBB", require_reply(replies, ReplyType.SYSTEM_STATUS))
            try:
                state = State(state)
            except ValueError as exc:
                raise ProtocolError(f"Unknown device state {state}") from exc
            self._update_active = state == State.UPDATE
            return DeviceStatus(state, bool(ready), written, crc, bool(valid), error)

    def get_memory_map(self):
        with self._lock:
            replies = self._request(Command.GET_MEMORY_MAP, retry=True)
            start, sector = struct.unpack("<IH", require_reply(replies, ReplyType.MEMORY_START))
            words, block = struct.unpack("<IH", require_reply(replies, ReplyType.MEMORY_SIZE))
            self.memory = MemoryMap(start, words, sector, block).validate()
            return self.memory

    def _layout(self):
        return self.memory if self.memory is not None else self.get_memory_map()

    def keep_alive(self):
        self._request(Command.KEEP_ALIVE, retry=True)

    def hold(self, seconds: float | None = None):
        """Keep the bootloader active until the duration elapses or Ctrl+C."""
        if seconds is not None and (not math.isfinite(seconds) or seconds < 0):
            raise ValueError("Hold duration must be nonnegative and finite")
        end = None if seconds is None else time.monotonic() + seconds
        while end is None or time.monotonic() < end:
            self.keep_alive()
            delay = self.keep_alive_interval if end is None else min(self.keep_alive_interval, max(0, end - time.monotonic()))
            self._stop.wait(delay)

    def read(self, address: int, words: int) -> bytes:
        with self._lock:
            memory = self._layout()
            memory.check_range(address, words)
            result = bytearray()
            for offset in range(0, words, MAX_READ_WORDS):
                count = min(MAX_READ_WORDS, words - offset)
                replies = self._request(Command.READ, struct.pack("<IH", address + offset, count), retry=True)
                chunks = {}
                for reply in replies:
                    if reply.kind != ReplyType.READ_DATA:
                        continue
                    index = struct.unpack_from("<H", reply.payload)[0]
                    if index % 2 or index >= count:
                        raise ProtocolError("Read reply contains an invalid word offset")
                    data = reply.payload[2:2 + min(2, count - index) * 2]
                    if index in chunks and chunks[index] != data:
                        raise ProtocolError("Conflicting read replies")
                    chunks[index] = data
                if set(chunks) != set(range(0, count, 2)):
                    raise ProtocolError("Read completed with missing data frames")
                result.extend(b"".join(chunks[index] for index in sorted(chunks)))
            return bytes(result)

    def erase(self, address: int | None = None, words: int | None = None):
        with self._lock:
            memory = self._layout()
            if address is None and words is None:
                self._request(Command.ERASE, long=True)
                return
            if address is None or words is None:
                raise ValueError("Range erase requires both address and word count")
            memory.check_range(address, words)
            if address % memory.sector_words or words % memory.sector_words:
                raise ValueError("Erase address and length must be sector aligned")
            maximum = (65535 // memory.sector_words) * memory.sector_words
            if not maximum:
                raise ProtocolError("Sector size does not fit a range-erase request")
            for offset in range(0, words, maximum):
                self._request(Command.ERASE_RANGE, struct.pack("<IH", address + offset, min(maximum, words - offset)), long=True)

    def begin_update(self, image_words: int, version: int):
        with self._lock:
            memory = self._layout()
            memory.check_range(memory.start, image_words)
            if not 0 <= version <= 65535:
                raise ValueError("Application version must fit 16 bits")
            self._request(Command.UPDATE_BEGIN, struct.pack("<IH", image_words, version))
            self._update_active = True

    def write(self, address: int, data: bytes):
        """Write erased application flash during an active update; pad to 8 words."""
        with self._lock:
            memory = self._layout()
            if not self._update_active:
                raise BootloaderError("Call begin_update before write")
            if not data or address % WRITE_ALIGNMENT:
                raise ValueError("Write requires data and an 8-word-aligned address")
            block_bytes = WRITE_ALIGNMENT * 2
            data = bytes(data).ljust(((len(data) + block_bytes - 1) // block_bytes) * block_bytes, b"\xff")
            memory.check_range(address, len(data) // 2)
            for byte_offset in range(0, len(data), memory.block_words * 2):
                block = data[byte_offset:byte_offset + memory.block_words * 2]
                block_address = address + byte_offset // 2
                for reload in range(self.retries + 1):
                    tag = self._new_tag()
                    self._last_block_tag = tag
                    self._request(Command.WRITE_BEGIN, struct.pack("<IH", block_address, len(block) // 2), tag=tag, retry=True)
                    for offset in range(0, len(block), 4):
                        chunk = block[offset:offset + 4]
                        frame = write_data(tag, offset // 2, chunk)
                        self._exchange(Command.WRITE_DATA, frame, retries=self.retries,
                                       expected_offset=(offset + len(chunk)) // 2)
                    try:
                        self._request(Command.WRITE_COMMIT, struct.pack("<I", zlib.crc32(block)), tag=tag, retry=True)
                        break
                    except CommandError as exc:
                        if exc.status != Status.RETRY or exc.error != Error.BLOCK_CRC or reload == self.retries:
                            raise
                        LOG.info("Reloading block at 0x%X after a CRC mismatch", block_address)
            return len(data) // 2

    def crc(self, address: int | None = None, words: int | None = None):
        with self._lock:
            memory = self._layout()
            if address is None and words is None:
                arguments = b""
            elif address is None or words is None:
                raise ValueError("Range CRC requires both address and word count")
            else:
                memory.check_range(address, words)
                if address == memory.start and words == memory.words:
                    arguments = b""
                elif words > 65535:
                    raise ValueError("Range CRC supports at most 65535 words; use the full-application CRC")
                else:
                    arguments = struct.pack("<IH", address, words)
            replies = self._request(Command.VERIFY, arguments, long=True, retry=True)
            return struct.unpack_from("<I", require_reply(replies, ReplyType.CRC))[0]

    def verify(self, expected: int, address: int | None = None, words: int | None = None):
        actual = self.crc(address, words)
        if actual != expected:
            raise VerificationError(expected, actual)
        return actual

    def finish_update(self, crc: int):
        with self._lock:
            if not self._update_active:
                raise BootloaderError("No update is active")
            self._end_session(Command.UPDATE_FINISH, struct.pack("<I", crc))
            self._update_active = False

    def abort_update(self):
        with self._lock:
            self._request(Command.UPDATE_ABORT)
            self._update_active = False

    def _end_session(self, command, arguments=b""):
        with self._lock:
            try:
                self._request(command, arguments, long=True)
            except CommandError:
                # An explicit rejection leaves the device available for commands.
                raise
            except BaseException:
                self._terminal = True
                self._stop.set()
                raise
            self._terminal = True
            self._stop.set()

    def run(self):
        self._end_session(Command.APP_RUN)

    def reset(self):
        self._end_session(Command.RESET)

    def flash_firmware(self, image: FirmwareImage, version: int,
                       progress: Callable[[int, int], None] | None = None):
        """Erase, write, verify, and finalize a complete application image."""
        with self._lock:
            self._check_health()
            memory = self._layout()
            prepared = image.prepare(memory)
            if not 0 <= version <= 65535:
                raise ValueError("Application version must fit 16 bits")
            if self.get_status().state != State.MENU:
                raise BootloaderError("Device must be in MENU; abort an existing update first")
            self.erase()
            attempted_begin = False
            try:
                attempted_begin = True
                self.begin_update(prepared.image_words, version)
                if progress:
                    progress(0, prepared.padded_words)
                for address, block in prepared.blocks(memory):
                    self.write(address, block)
                    if progress:
                        progress(address - memory.start + len(block) // 2, prepared.padded_words)
                self.verify(prepared.crc)
                if progress:
                    progress(prepared.padded_words, prepared.padded_words)
                # Do not send ABORT if FINISH was sent: the board may have reset.
                attempted_begin = False
                self.finish_update(prepared.crc)
            except BaseException:
                if attempted_begin:
                    try:
                        self.abort_update()
                    except BootloaderError:
                        LOG.warning("Could not abort the interrupted update; reset/query the device", exc_info=True)
                raise
            return prepared.crc
