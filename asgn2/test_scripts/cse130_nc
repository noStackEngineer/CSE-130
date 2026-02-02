#!/usr/bin/env python3

import argparse, socket, sys, os

def main():

    parser = argparse.ArgumentParser(description="my nc")
    parser.add_argument(
        "hostname", metavar="HOSTNAME", help="Host to connect with")
    parser.add_argument(
        "port", metavar="PORT", type=int, help="port to connect with")
    parser.add_argument(
        "-l", "--newlines", required=False, action="store_true",
        default=False, help="add CRLF")
    args = parser.parse_args()

    total = 0

    # Import assignment script early on; better for catching errors.
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((args.hostname, args.port))

        while True:
            data = sys.stdin.buffer.read(4096)
            if len(data) == 0:
                break
            total += len(data)
            # print("=" * 40, file=sys.stderr)
            # print(f"total = {total}", file=sys.stderr)
            # print(f"bytes = {len(data)}", file=sys.stderr)
            # print("=" * 40, file=sys.stderr)
            s.sendall(data)
        s.shutdown(socket.SHUT_WR)

        data = s.recv(1024)
        while data:
            sys.stdout.buffer.write(data)
            data = s.recv(1024)
        sys.stdout.buffer.flush()

if __name__ == "__main__":
    main()

