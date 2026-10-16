"""Python client tests, including integration with the production C firmware."""
from collections import deque
from contextlib import redirect_stderr, redirect_stdout
import ctypes
import importlib.util
import io
import os
from pathlib import Path
import shutil
import struct
import tempfile
import threading
import time
import unittest
from unittest.mock import MagicMock, patch
import uuid
import zlib

import can

from canboot import (BootloaderClient, BootloaderError, CommandError, FirmwareImage,
                     ProtocolError, ReplyTimeout, TransportError, VerificationError)
from canboot.cli import main
from canboot.protocol import Command, Frame, MemoryMap, Reply, ReplyType, request, write_data
from canboot.transport import PythonCanTransport

ROOT = Path(__file__).resolve().parents[2]
BASE, WORDS = 0x85000, 0x2B000
COMPILER = shutil.which(os.environ.get("CC", "gcc"))
if COMPILER is None and Path("C:/msys64/ucrt64/bin/gcc.exe").is_file():
    COMPILER = "C:/msys64/ucrt64/bin/gcc.exe"


class ImageTests(unittest.TestCase):
    memory = MemoryMap(BASE, 256, 16, 128)

    def test_sparse_word_addresses_padding_and_crc(self):
        image = FirmwareImage(((BASE, b"\x34\x12\x56"), (BASE + 129, b"\xcd\xab")))
        prepared = image.prepare(self.memory)
        self.assertEqual(prepared.region[:4], b"\x34\x12\x56\xff")
        self.assertEqual(prepared.region[4:258], b"\xff" * 254)
        self.assertEqual(prepared.region[258:260], b"\xcd\xab")
        self.assertEqual((prepared.image_words, prepared.padded_words), (130, 136))
        self.assertEqual(prepared.crc, zlib.crc32(prepared.region))
        self.assertEqual([len(data) for _, data in prepared.blocks(self.memory)], [256, 16])

    def test_empty_overlapping_or_out_of_range_images(self):
        images = [FirmwareImage(()), FirmwareImage(((BASE, b""),)),
                  FirmwareImage(((BASE - 1, b"\0\0"),)),
                  FirmwareImage(((BASE + 256, b"\0\0"),)),
                  FirmwareImage(((BASE, b"\0\0\0"), (BASE + 1, b"\0\0"))),
                  FirmwareImage(((BASE + 1, b"\0\0"),))]
        for image in images:
            with self.subTest(image=image), self.assertRaises(ValueError):
                image.prepare(self.memory)

    def test_binary_loader_rejects_elf_and_hex(self):
        with self.assertRaises(ValueError): FirmwareImage.from_binary(b"\x7fELFdata", BASE)
        with self.assertRaises(ValueError): FirmwareImage.from_binary(b"", BASE)
        with self.assertRaises(ValueError): FirmwareImage.from_file("firmware.out", BASE)
        with self.assertRaises(ValueError): FirmwareImage.from_file("firmware.hex", BASE)

    def test_erased_blocks_are_skipped(self):
        prepared = FirmwareImage(((BASE, b"\x34\x12"), (BASE + 255, b"\xff\xff"))).prepare(self.memory)
        self.assertEqual(len(list(prepared.blocks(self.memory))), 1)

    def test_known_word_order(self):
        self.assertEqual(request(Command.READ, 2, struct.pack("<IH", BASE, 8)).data,
                         b"\x00\x50\x08\x00\x08\x00\x02\x0c")
        self.assertEqual(write_data(3, 2, b"\x34\x12\xcd\xab").data,
                         b"\x02\x00\x34\x12\xcd\xab\x03\x02")
        with self.assertRaises(ValueError): write_data(3, 0, b"\0")
        with self.assertRaises(ProtocolError): Reply.decode(b"\0" * 7)


class TransportTests(unittest.TestCase):
    def test_pcan_configuration_and_shutdown(self):
        bus = MagicMock()
        with patch("canboot.transport.can.Bus", return_value=bus) as factory:
            transport = PythonCanTransport.pcan(device_id=17)
        options = factory.call_args.kwargs
        self.assertEqual((options["interface"], options["channel"], options["bitrate"]),
                         ("pcan", "PCAN_USBBUS1", 500000))
        self.assertEqual(options["device_id"], 17)
        self.assertTrue(options["ignore_config"])
        self.assertFalse(options["fd"])
        transport.send(request(Command.GET_INFO, 1), 0.2)
        self.assertFalse(bus.send.call_args.args[0].is_extended_id)
        transport.close()
        bus.shutdown.assert_called_once()

    def test_pcan_driver_failure_is_actionable(self):
        with patch("canboot.transport.can.Bus", side_effect=can.CanInitializationError("missing DLL")):
            with self.assertRaisesRegex(TransportError, "PEAK driver"):
                PythonCanTransport.pcan()

    def test_bus_errors_are_reported(self):
        bus = MagicMock()
        bus.recv.return_value = can.Message(is_error_frame=True)
        with self.assertRaises(TransportError): PythonCanTransport(bus).receive(0.1)
        bus.send.side_effect = can.CanOperationError("bus off")
        with self.assertRaises(TransportError): PythonCanTransport(bus).send(request(Command.GET_INFO, 1), 0.1)

    def test_bus_noise_cannot_extend_deadline(self):
        class Noise:
            def send(self, frame, timeout): pass
            def receive(self, timeout): return Frame(0x200, b"\0" * 8)
            def close(self): pass
        client = BootloaderClient(Noise(), timeout=0.01, retries=0, auto_keep_alive=False)
        started = time.monotonic()
        with self.assertRaises(ReplyTimeout): client.keep_alive()
        self.assertLess(time.monotonic() - started, 0.5)
        client.close()


class FirmwareTransport:
    """Bridge Python frames to the real C decoder/state machine/flash buffer."""
    def __init__(self, library):
        self.lib = library
        self.queue = deque()
        self.sent = []
        self.transform = lambda frame, reply: [reply]
        self.wire_transform = lambda frame: frame
        self.closed = False

    def send(self, frame, timeout):
        self.sent.append(frame)
        self.lib.test_clear()
        wire = self.wire_transform(frame)
        self.lib.test_frame(wire.arbitration_id, wire.data, len(wire.data))
        if self.lib.test_state() == 5:
            self.lib.test_step()  # The MCU main loop performs error recovery.
        for index in range(self.lib.test_count()):
            payload = (ctypes.c_ubyte * 8)()
            self.lib.test_reply(index, payload)
            self.queue.extend(self.transform(frame, Frame(0x200, bytes(payload))))

    def receive(self, timeout):
        if self.queue:
            return self.queue.popleft()
        time.sleep(min(timeout, 0.002))
        # A real recv waits until its deadline; return None means that timeout.
        return None

    def close(self):
        self.closed = True

    def commands(self):
        return [Command(frame.data[7]) for frame in self.sent if frame.arbitration_id == 0x201]


@unittest.skipUnless(COMPILER, "Set CC to GCC to run tests against the C firmware")
class FirmwareIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("firmware_commands", ROOT / "tests/test_commands.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        cls.build = module.firmware_library(COMPILER)
        cls.lib = cls.build.__enter__()

    @classmethod
    def tearDownClass(cls):
        cls.build.__exit__(None, None, None)

    def setUp(self):
        self.lib.test_reset(0)
        self.transport = FirmwareTransport(self.lib)
        self.client = BootloaderClient(self.transport, timeout=0.02, operation_timeout=0.03, auto_keep_alive=False)
        self.client.connect()

    def tearDown(self):
        self.client.close()

    def drop_once(self, command):
        dropped = False

        def transform(sent, reply):
            nonlocal dropped
            if (not dropped and reply.data[7] == 0 and reply.data[0] == 1 and reply.data[1] == command):
                dropped = True
                return []
            return [reply]
        self.transport.transform = transform

    def image(self, words=300):
        return FirmwareImage.from_binary(struct.pack("<" + "H" * words, *range(words)), BASE)

    def test_negotiation_and_full_update(self):
        self.assertEqual((self.client.device.protocol_version, self.client.device.serial_number), (2, 42))
        self.assertEqual(self.client.memory.start, BASE)
        image = self.image()
        progress = []
        crc = self.client.flash_firmware(image, 12, lambda done, total: progress.append((done, total)))
        self.assertEqual(crc, image.prepare(self.client.memory).crc)
        self.assertEqual(crc, self.lib.test_saved_crc())
        self.assertEqual(self.lib.test_writes(), 3)
        self.assertEqual(self.lib.test_write_length(), 48)
        self.assertEqual(self.transport.commands().count(Command.UPDATE_FINISH), 1)
        self.assertEqual(progress[-1], (304, 304))

    def test_read_splits_requests_and_retains_odd_tail(self):
        self.client.begin_update(8, 1)
        self.client.write(BASE, b"\x34\x12\xcd\xab")
        data = self.client.read(BASE, 257)
        self.assertEqual(data, b"\x34\x12\xcd\xab" + b"\xff" * 510)
        self.assertEqual(self.transport.commands().count(Command.READ), 3)
        self.client.abort_update()

    def test_range_erase_splits_at_wire_length_limit(self):
        self.client.erase(BASE, 32 * 4096)
        self.assertEqual(self.transport.commands().count(Command.ERASE_RANGE), 3)

    def test_invalid_image_is_rejected_before_erase(self):
        image = FirmwareImage(((BASE - 8, b"\0\0"),))
        with self.assertRaises(ValueError): self.client.flash_firmware(image, 1)
        self.assertEqual(self.lib.test_erases(), 0)

    def test_lost_data_ack_replays_without_appending_twice(self):
        self.drop_once(Command.WRITE_DATA)
        self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.lib.test_writes(), 1)
        self.assertEqual(len([frame for frame in self.transport.sent if frame.arbitration_id == 0x203]), 5)

    def test_lost_commit_ack_does_not_program_twice(self):
        self.drop_once(Command.WRITE_COMMIT)
        self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.lib.test_writes(), 1)
        self.assertEqual(self.transport.commands().count(Command.WRITE_COMMIT), 2)

    def test_lost_begin_block_ack_preserves_block(self):
        self.drop_once(Command.WRITE_BEGIN)
        self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.lib.test_writes(), 1)
        self.assertEqual(self.transport.commands().count(Command.WRITE_BEGIN), 2)

    def test_delayed_data_ack_is_not_mistaken_for_next_ack(self):
        def transform(sent, reply):
            if sent.arbitration_id == 0x203 and reply.data[7] == 0:
                offset = struct.unpack_from("<H", sent.data)[0]
                if offset:
                    stale = bytearray(reply.data)
                    struct.pack_into("<H", stale, 4, offset)
                    return [Frame(0x200, bytes(stale)), reply]
            return [reply]
        self.transport.transform = transform
        self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.lib.test_writes(), 1)

    def test_lost_erase_ack_is_not_retried(self):
        self.drop_once(Command.ERASE)
        with self.assertRaises(ReplyTimeout): self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.lib.test_erases(), 1)
        self.assertNotIn(Command.UPDATE_BEGIN, self.transport.commands())

    def test_lost_update_begin_ack_aborts_instead_of_repeating(self):
        self.drop_once(Command.UPDATE_BEGIN)
        with self.assertRaises(ReplyTimeout): self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.transport.commands().count(Command.UPDATE_BEGIN), 1)
        self.assertEqual(self.transport.commands().count(Command.UPDATE_ABORT), 1)
        self.assertEqual(self.lib.test_state(), 1)

    def test_lost_finish_ack_has_no_abort_or_repeat(self):
        self.drop_once(Command.UPDATE_FINISH)
        with self.assertRaises(ReplyTimeout): self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.transport.commands().count(Command.UPDATE_FINISH), 1)
        self.assertNotIn(Command.UPDATE_ABORT, self.transport.commands())
        self.assertEqual(self.lib.test_saved_crc(), self.lib.test_crc())

    def test_crc_mismatch_aborts_before_metadata_commit(self):
        def transform(sent, reply):
            if reply.data[7] == ReplyType.CRC:
                corrupt = bytearray(reply.data)
                corrupt[0] ^= 1
                return [Frame(0x200, bytes(corrupt))]
            return [reply]
        self.transport.transform = transform
        with self.assertRaises(VerificationError): self.client.flash_firmware(self.image(8), 1)
        self.assertNotIn(Command.UPDATE_FINISH, self.transport.commands())
        self.assertIn(Command.UPDATE_ABORT, self.transport.commands())

    def test_missing_read_frame_is_reported(self):
        self.transport.transform = lambda sent, reply: [] if reply.data[7] == ReplyType.READ_DATA and reply.data[:2] == b"\0\0" else [reply]
        with self.assertRaisesRegex(ProtocolError, "missing data"): self.client.read(BASE, 8)

    def test_incompatible_protocol_rejected_without_erase(self):
        def transform(sent, reply):
            if reply.data[7] == ReplyType.CAPABILITIES:
                data = bytearray(reply.data)
                data[0] = 99
                return [Frame(0x200, bytes(data))]
            return [reply]
        self.transport.transform = transform
        with self.assertRaises(ProtocolError): self.client.connect()
        self.assertEqual(self.lib.test_erases(), 0)

    def test_tag_wrap_and_nonzero_block_tags(self):
        for _ in range(260): self.client.keep_alive()
        self.client.flash_firmware(self.image(8), 1)
        self.assertTrue(all(frame.data[6] != 0 for frame in self.transport.sent))

    def test_rejected_run_leaves_client_usable(self):
        with self.assertRaises(CommandError): self.client.run()
        self.assertEqual(self.client.get_status().state.name, "MENU")

    def test_block_crc_retry_reloads_with_a_new_tag(self):
        corrupted = False

        def corrupt_first_commit(frame):
            nonlocal corrupted
            if not corrupted and frame.arbitration_id == 0x201 and frame.data[7] == Command.WRITE_COMMIT:
                corrupted = True
                data = bytearray(frame.data)
                data[0] ^= 1
                return Frame(frame.arbitration_id, bytes(data))
            return frame
        self.transport.wire_transform = corrupt_first_commit
        self.client.flash_firmware(self.image(8), 1)
        tags = [frame.data[6] for frame in self.transport.sent if frame.arbitration_id == 0x201 and frame.data[7] == Command.WRITE_BEGIN]
        self.assertEqual(len(tags), 2)
        self.assertNotEqual(tags[0], tags[1])
        self.assertEqual(self.lib.test_writes(), 1)

    def test_interrupt_aborts_update_without_metadata_commit(self):
        def interrupt(done, total):
            if done: raise KeyboardInterrupt
        with self.assertRaises(KeyboardInterrupt): self.client.flash_firmware(self.image(), 1, interrupt)
        self.assertIn(Command.UPDATE_ABORT, self.transport.commands())
        self.assertNotIn(Command.UPDATE_FINISH, self.transport.commands())
        self.assertEqual(self.lib.test_metadata_erases(), 0)
        self.assertEqual(self.lib.test_state(), 1)

    def test_metadata_failure_remains_an_explicit_failure(self):
        self.lib.test_metadata_failure(2)
        with self.assertRaises(CommandError): self.client.flash_firmware(self.image(8), 1)
        self.assertEqual(self.transport.commands().count(Command.UPDATE_FINISH), 1)
        self.assertFalse(self.client.get_status().app_valid)

    def test_cli_flash_runs_against_firmware(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "application.bin"
            image.write_bytes(struct.pack("<8H", *range(8)))
            output = io.StringIO()
            with patch("canboot.cli.BootloaderClient.open_pcan", return_value=self.client), redirect_stdout(output), redirect_stderr(io.StringIO()):
                self.assertEqual(main(["write", str(image), "--version", "4"]), 0)
            self.assertIn("flashed; device resetting", output.getvalue())
            self.assertEqual(self.lib.test_saved_crc(), self.lib.test_crc())

    def test_failed_background_keep_alive_is_surfaced(self):
        self.client.close()
        self.transport = FirmwareTransport(self.lib)
        event = threading.Event()
        send = self.transport.send

        def fail_keep_alive(frame, timeout):
            if frame.arbitration_id == 0x201 and frame.data[7] == Command.KEEP_ALIVE:
                event.set()
                raise TransportError("disconnected")
            send(frame, timeout)
        self.transport.send = fail_keep_alive
        self.client = BootloaderClient(self.transport, auto_keep_alive=True, keep_alive_interval=0.03)
        self.client.connect()
        self.assertTrue(event.wait(1))
        with self.assertRaisesRegex(BootloaderError, "Keep-alive failed"):
            self.client.get_status()

    def test_keep_alive_serialization_and_shutdown(self):
        self.client.close()
        self.transport = FirmwareTransport(self.lib)
        self.client = BootloaderClient(self.transport, timeout=0.02, auto_keep_alive=True, keep_alive_interval=0.03)
        event = threading.Event()

        def transform(sent, reply):
            if reply.data[7] == 0 and reply.data[1] == Command.KEEP_ALIVE:
                event.set()
            return [reply]
        self.transport.transform = transform
        self.client.connect()
        self.assertTrue(event.wait(1))
        for _ in range(20): self.client.get_status()
        self.client.close()
        count = len(self.transport.sent)
        time.sleep(0.05)
        self.assertEqual(len(self.transport.sent), count)
        self.assertTrue(self.transport.closed)

    def test_python_can_virtual_bus_to_real_firmware(self):
        channel = f"canboot-test-{uuid.uuid4()}"
        host = can.Bus(interface="virtual", channel=channel, ignore_config=True)
        board = can.Bus(interface="virtual", channel=channel, ignore_config=True)
        bridge = FirmwareTransport(self.lib)
        stop = threading.Event()

        def serve():
            while not stop.is_set():
                incoming = board.recv(0.01)
                if incoming is None: continue
                bridge.send(Frame(incoming.arbitration_id, bytes(incoming.data)), 0.1)
                while bridge.queue:
                    frame = bridge.queue.popleft()
                    board.send(can.Message(arbitration_id=frame.arbitration_id, data=frame.data, is_extended_id=False))
        worker = threading.Thread(target=serve)
        worker.start()
        try:
            with BootloaderClient(PythonCanTransport(host), auto_keep_alive=False) as client:
                crc = client.flash_firmware(self.image(8), 1)
                self.assertEqual(crc, self.lib.test_saved_crc())
        finally:
            stop.set()
            worker.join()
            host.shutdown()
            board.shutdown()


class CliTests(unittest.TestCase):
    def test_bad_erase_options_do_not_open_adapter(self):
        with patch("canboot.cli.BootloaderClient.open_pcan") as factory, redirect_stderr(io.StringIO()):
            self.assertEqual(main(["erase", "--address", "0x85000"]), 1)
            factory.assert_not_called()

    def test_read_writes_little_endian_output(self):
        client = MagicMock()
        client.read.return_value = b"\x34\x12\xcd\xab"
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "read.bin"
            with patch("canboot.cli.BootloaderClient.open_pcan") as factory, redirect_stdout(io.StringIO()):
                factory.return_value.__enter__.return_value = client
                self.assertEqual(main(["--channel", "PCAN_USBBUS2", "read", "0x85000", "2", str(output)]), 0)
                self.assertEqual(factory.call_args.kwargs["channel"], "PCAN_USBBUS2")
            client.read.assert_called_once_with(BASE, 2)
            self.assertEqual(output.read_bytes(), b"\x34\x12\xcd\xab")

    def test_verification_failure_has_nonzero_exit(self):
        with patch("canboot.cli.BootloaderClient.open_pcan") as factory, redirect_stderr(io.StringIO()):
            factory.return_value.__enter__.return_value.verify.side_effect = VerificationError(1, 2)
            self.assertEqual(main(["verify", "--expected", "1"]), 1)


if __name__ == "__main__":
    unittest.main()
