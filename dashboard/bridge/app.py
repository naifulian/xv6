"""Minimal host bridge for the module four runtime dashboard."""

from __future__ import annotations

import argparse
import json
import mimetypes
from functools import partial
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

from .parser import build_process_detail
from .state import DashboardState


class DashboardHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, directory: str, state: DashboardState, **kwargs):
        self.dashboard_state = state
        super().__init__(*args, directory=directory, **kwargs)

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path.startswith("/api/"):
            self._handle_api(parsed.path)
            return
        if parsed.path == "/":
            self.path = "/index.html"
        super().do_GET()

    def log_message(self, format: str, *args) -> None:
        return

    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def _handle_api(self, path: str) -> None:
        payload = self.dashboard_state.get_payload()
        routes = {
            "/api/dashboard": payload,
            "/api/summary": payload.get("summary"),
            "/api/processes": payload.get("latest_processes"),
            "/api/events": payload.get("events"),
            "/api/policy": payload.get("summary", {}).get("policy"),
        }

        if path in routes:
            self._send_json(routes[path])
            return

        if path.startswith("/api/processes/"):
            pid_text = path.rsplit("/", 1)[-1]
            try:
                pid = int(pid_text)
            except ValueError:
                self.send_error(HTTPStatus.BAD_REQUEST, "invalid pid")
                return

            details = payload.get("process_details") or {}
            detail = details.get(str(pid))
            if detail:
                self._send_json(detail)
                return

            summary = payload.get("summary") or {}
            for proc in payload.get("latest_processes", []):
                if proc.get("pid") == pid:
                    self._send_json(build_process_detail(proc, summary) or proc)
                    return
            self.send_error(HTTPStatus.NOT_FOUND, "process not found")
            return

        self.send_error(HTTPStatus.NOT_FOUND, "unknown api route")

    def _send_json(self, body) -> None:
        encoded = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Serve the xv6 runtime dashboard bridge")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--source", default="dashboard/runtime/dashboard-telemetry.log")
    parser.add_argument("--root", default="dashboard")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    mimetypes.add_type("application/javascript", ".js")

    state = DashboardState(args.source)
    handler = partial(DashboardHandler, directory=str(root), state=state)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    print(f"Serving module four dashboard on http://{args.host}:{args.port}")
    print(f"Telemetry source: {Path(args.source).resolve()}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
