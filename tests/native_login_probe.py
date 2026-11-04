"""Exercise the native executable over a real fragmented loopback A101 socket."""
import socket
import struct
import subprocess
import sys
import threading


def main():
    failures = []
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(8)
    port = listener.getsockname()[1]

    def server():
        try:
            connection, _ = listener.accept()
            with connection:
                connection.settimeout(8)
                exponent = (65537).to_bytes(64, "little")
                modulus = ((1 << 1024) - 109).to_bytes(128, "little")
                body = bytes((0, 3, 128)) + exponent + modulus
                challenge = struct.pack("<HH", len(body) + 4, 0xA101) + body
                # A TCP read is not a packet boundary.
                for offset in range(0, len(challenge), 7):
                    connection.sendall(challenge[offset:offset + 7])
                data = bytearray()
                while len(data) < 133:
                    chunk = connection.recv(133 - len(data))
                    if not chunk:
                        raise RuntimeError("Client closed before its A101 response")
                    data.extend(chunk)
                length, opcode = struct.unpack_from("<HH", data)
                if length != 133 or opcode != 0xA101 or data[4] > 128 or data[4] % 4:
                    raise RuntimeError("Invalid recovered A101 response framing")
        except Exception as error:
            failures.append(str(error))

    worker = threading.Thread(target=server)
    worker.start()
    try:
        result = subprocess.run(
            [*sys.argv[1:], "127.0.0.1", str(port)],
            capture_output=True, text=True, timeout=12,
        )
    finally:
        worker.join(timeout=9)
        listener.close()
    if result.returncode or failures or "A101 exchange completed" not in result.stdout:
        print(result.stdout, result.stderr, failures, file=sys.stderr)
        return 1
    print("Native executable A101 loopback exchange passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
