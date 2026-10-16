"""python-can transport, with a PCAN factory and an injectable bus for tests."""
import logging
from typing import Protocol

import can

from .errors import TransportError
from .protocol import Frame, REPLY_ID

LOG = logging.getLogger(__name__)


class Transport(Protocol):
    def send(self, frame: Frame, timeout: float) -> None: ...
    def receive(self, timeout: float) -> Frame | None: ...
    def close(self) -> None: ...


class PythonCanTransport:
    def __init__(self, bus: can.BusABC):
        self.bus = bus

    @classmethod
    def pcan(cls, channel="PCAN_USBBUS1", bitrate=500000, device_id=None):
        options = dict(interface="pcan", channel=channel, bitrate=bitrate, fd=False,
                       ignore_config=True, receive_own_messages=False,
                       can_filters=[{"can_id": REPLY_ID, "can_mask": 0x7FF, "extended": False}])
        if device_id is not None:
            options["device_id"] = device_id
        try:
            return cls(can.Bus(**options))
        except (can.CanError, OSError, ImportError, ValueError) as exc:
            raise TransportError(f"Cannot open PCAN channel {channel}: {exc}. "
                                 "Check the PEAK driver, PCAN-Basic installation, and channel.") from exc

    def send(self, frame: Frame, timeout: float):
        LOG.debug("TX %03X %s", frame.arbitration_id, frame.data.hex(" "))
        try:
            self.bus.send(can.Message(arbitration_id=frame.arbitration_id, data=frame.data,
                                      is_extended_id=False, check=True), timeout=timeout)
        except (can.CanError, OSError) as exc:
            raise TransportError(f"CAN send failed: {exc}") from exc

    def receive(self, timeout: float):
        try:
            message = self.bus.recv(timeout=timeout)
        except (can.CanError, OSError) as exc:
            raise TransportError(f"CAN receive failed: {exc}") from exc
        if message is None:
            return None
        LOG.debug("RX %03X %s", message.arbitration_id, message.data.hex(" "))
        if message.is_error_frame:
            raise TransportError("CAN bus reported an error frame")
        return Frame(message.arbitration_id, bytes(message.data), message.is_extended_id,
                     message.is_error_frame, message.is_remote_frame, message.is_fd)

    def close(self):
        try:
            self.bus.shutdown()
        except (can.CanError, OSError) as exc:
            raise TransportError(f"CAN shutdown failed: {exc}") from exc
