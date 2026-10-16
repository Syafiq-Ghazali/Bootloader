"""Errors exposed by the Python client."""


class BootloaderError(Exception):
    """Base error for communication, protocol, and device failures."""


class TransportError(BootloaderError):
    """CAN adapter or bus operation failed."""


class ReplyTimeout(BootloaderError):
    """The outcome of a request could not be confirmed."""


class ProtocolError(BootloaderError):
    """A device reply is incomplete, incompatible, or inconsistent."""


class CommandError(BootloaderError):
    def __init__(self, command, status, error, offset=0):
        self.command = command
        self.status = status
        self.error = error
        self.offset = offset
        super().__init__(f"{command.name}: {status.name}, {error.name} (next word {offset})")


class VerificationError(BootloaderError):
    def __init__(self, expected: int, actual: int):
        self.expected = expected
        self.actual = actual
        super().__init__(f"CRC mismatch: expected 0x{expected:08X}, received 0x{actual:08X}")
