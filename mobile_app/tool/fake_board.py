#!/usr/bin/env python3
"""Fake face-unlock board: serves the same HTTP endpoints as the firmware's
video_stream component, so the app can be tested without hardware.

  /        small viewer page
  /stream  MJPEG, same part format and boundary as the firmware
  /jpg     one frame
  One request at a time, like esp_http_server: while someone watches
  /stream, other requests get no answer until that viewer leaves.

Frames are the JPEG files in tool/fake_frames/, played in a loop.
Standard library only.

  python3 tool/fake_board.py --port 8080
"""

import argparse
import pathlib
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

PART_BOUNDARY = "frameboundary"
VIEWER_PAGE = (
    b"<!doctype html><html><body style='background:#111;color:#eee'>"
    b"<h3>Fake face unlock board</h3><img src='/stream'></body></html>"
)

frames = []
frame_delay_s = 0.1


class BoardHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            self.send_body(200, "text/html", VIEWER_PAGE)
        elif self.path == "/jpg":
            frame = frames[int(time.time() / frame_delay_s) % len(frames)]
            self.send_body(200, "image/jpeg", frame)
        elif self.path == "/stream":
            self.send_stream()
        else:
            self.send_body(404, "text/plain", b"not found")

    def send_body(self, status, content_type, body):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_stream(self):
        try:
            self.send_response(200)
            self.send_header(
                "Content-Type", "multipart/x-mixed-replace;boundary=" + PART_BOUNDARY
            )
            self.end_headers()
            frame_index = 0
            while True:
                frame = frames[frame_index % len(frames)]
                part_header = (
                    f"\r\n--{PART_BOUNDARY}\r\n"
                    f"Content-Type: image/jpeg\r\nContent-Length: {len(frame)}\r\n\r\n"
                )
                self.wfile.write(part_header.encode("ascii"))
                self.wfile.write(frame)
                self.wfile.flush()
                frame_index += 1
                time.sleep(frame_delay_s)
        except (BrokenPipeError, ConnectionResetError):
            pass  # viewer closed the stream
        print("viewer gone")


def main():
    global frame_delay_s
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--fps", type=int, default=10)
    parser.add_argument(
        "--frames",
        type=pathlib.Path,
        default=pathlib.Path(__file__).parent / "fake_frames",
        help="folder of .jpg files to play in a loop",
    )
    args = parser.parse_args()

    for path in sorted(args.frames.glob("*.jpg")):
        frames.append(path.read_bytes())
    if not frames:
        parser.error(f"no .jpg files in {args.frames}")
    frame_delay_s = 1.0 / args.fps

    # Not ThreadingHTTPServer: the board answers one request at a time.
    server = HTTPServer(("0.0.0.0", args.port), BoardHandler)
    print(f"fake board on http://0.0.0.0:{args.port}/  ({len(frames)} frames)")
    server.serve_forever()


if __name__ == "__main__":
    main()
