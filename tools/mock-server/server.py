"""
HerculesAC mock backend server — for local development and integration testing.

Usage:
    pip install flask
    python server.py [--port 8443]

The server listens on HTTP (no TLS) by default so you can configure
Reporter with http://localhost:8443/v1/events during development.

Query parameters on POST /v1/events:
    ?fail=4xx   — return 400 Bad Request (simulates drop)
    ?fail=5xx   — return 503 Service Unavailable (simulates retry)
    ?fail=429   — return 429 Too Many Requests with Retry-After: 5
"""

import argparse
import json
import logging
from datetime import datetime

from flask import Flask, jsonify, request

app = Flask(__name__)
log = logging.getLogger("mock-server")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")

received: list[dict] = []


@app.post("/v1/events")
def ingest():
    fail = request.args.get("fail", "")
    if fail == "4xx":
        log.info("Simulating 400 Bad Request")
        return jsonify({"error": "simulated 4xx"}), 400
    if fail == "5xx":
        log.info("Simulating 503 Service Unavailable")
        return jsonify({"error": "simulated 5xx"}), 503
    if fail == "429":
        log.info("Simulating 429 Too Many Requests")
        return jsonify({"error": "rate limited"}), 429, {"Retry-After": "5"}

    body = request.get_json(force=True, silent=True) or []
    if not isinstance(body, list):
        return jsonify({"error": "body must be a JSON array"}), 400

    for ev in body:
        ev["_received_at"] = datetime.utcnow().isoformat() + "Z"
        received.append(ev)
        log.info(
            "Event kind=%s severity=%s client=%s",
            ev.get("kind"),
            ev.get("severity"),
            ev.get("client_id", "?")[:12],
        )

    return jsonify({"accepted": len(body)}), 200


@app.get("/v1/events")
def dump():
    """Return all received events as a JSON array."""
    return jsonify(received), 200


@app.delete("/v1/events")
def clear():
    """Clear the in-memory event log."""
    received.clear()
    return jsonify({"cleared": True}), 200


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    log.info("Mock server listening on http://0.0.0.0:%d", args.port)
    app.run(host="0.0.0.0", port=args.port, debug=False)
