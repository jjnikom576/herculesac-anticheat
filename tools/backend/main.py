"""
HerculesAC Telemetry Backend
Usage:
    pip install -r requirements.txt
    uvicorn main:app --host 0.0.0.0 --port 8080 --reload

API
    POST /v1/events          Accept AntiCheatEvent JSON array from Reporter
    GET  /v1/events          List stored events (query: limit, kind, severity, since)
    DELETE /v1/events        Clear all events
    GET  /dashboard          Live HTML dashboard
    GET  /metrics            Simple Prometheus-style counters
"""

import json
import os
import time
import asyncio
import sqlite3
import aiosqlite
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional, List

from fastapi import FastAPI, Request, Query, Header, HTTPException, Depends
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.templating import Jinja2Templates

# ── Config ────────────────────────────────────────────────────────────────────
DB_PATH   = Path(__file__).parent / "events.db"
TEMPLATES = Jinja2Templates(directory=str(Path(__file__).parent / "templates"))

# API key: set HAC_API_KEY env var before starting the server.
# If unset the server refuses all write/delete requests (fail-secure).
_API_KEY: str = os.environ.get("HAC_API_KEY", "")

app = FastAPI(title="HerculesAC Telemetry", version="1.1.0")

# ── Auth ──────────────────────────────────────────────────────────────────────
def require_api_key(x_api_key: str = Header(default="")):
    """Dependency: enforce X-Api-Key header on write / destructive endpoints."""
    if not _API_KEY:
        raise HTTPException(503, "Server not configured: HAC_API_KEY env var is unset")
    if x_api_key != _API_KEY:
        raise HTTPException(403, "Invalid or missing X-Api-Key header")

# ── DB init ───────────────────────────────────────────────────────────────────
async def init_db():
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("""
            CREATE TABLE IF NOT EXISTS events (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                received_at     INTEGER NOT NULL,
                client_id       TEXT,
                session_id      TEXT,
                game_id         TEXT,
                timestamp_utc   INTEGER,
                severity        INTEGER,
                kind            INTEGER,
                detector_ver    TEXT,
                evidence_json   TEXT,
                raw             TEXT NOT NULL
            )
        """)
        await db.execute("CREATE INDEX IF NOT EXISTS idx_received ON events(received_at DESC)")
        await db.execute("CREATE INDEX IF NOT EXISTS idx_kind     ON events(kind)")
        await db.execute("CREATE INDEX IF NOT EXISTS idx_severity ON events(severity)")
        await db.commit()

@app.on_event("startup")
async def startup():
    await init_db()

# ── Helpers ───────────────────────────────────────────────────────────────────
SEVERITY_MAP = {1: "Info", 2: "Warn", 3: "Critical"}
KIND_MAP     = {
    0: "Unknown",        1: "ProcName",       2: "WinName",
    3: "ModuleHash",     4: "RpmRate",        5: "WpmTarget",
    6: "HookIntegrity",  7: "IatHook",        8: "EatHook",
    9: "CodeSectionDiverge", 10: "ThreadInject", 11: "KnownBadDriver",
}

def row_to_dict(row):
    return {
        "id":           row[0],
        "received_at":  row[1],
        "client_id":    row[2],
        "session_id":   row[3],
        "game_id":      row[4],
        "timestamp_utc": row[5],
        "severity":     SEVERITY_MAP.get(row[6], str(row[6])),
        "kind":         KIND_MAP.get(row[7], str(row[7])),
        "detector_ver": row[8],
        "evidence":     row[9],
    }

# ── POST /v1/events ───────────────────────────────────────────────────────────
@app.post("/v1/events", status_code=204, dependencies=[Depends(require_api_key)])
async def ingest_events(request: Request):
    body = await request.body()
    try:
        events = json.loads(body)
        if not isinstance(events, list):
            events = [events]
    except Exception:
        raise HTTPException(400, "invalid JSON")

    now = int(time.time() * 1000)
    async with aiosqlite.connect(DB_PATH) as db:
        for ev in events:
            await db.execute("""
                INSERT INTO events
                    (received_at, client_id, session_id, game_id,
                     timestamp_utc, severity, kind, detector_ver, evidence_json, raw)
                VALUES (?,?,?,?,?,?,?,?,?,?)
            """, (
                now,
                ev.get("client_id", ""),
                ev.get("session_id", ""),
                ev.get("game_id", ""),
                ev.get("timestamp_utc_ms", 0),
                ev.get("severity", 0),
                ev.get("kind", 0),
                ev.get("detector_version", ""),
                ev.get("evidence_json", ""),
                json.dumps(ev),
            ))
        await db.commit()
    return JSONResponse(None, status_code=204)

# ── GET /v1/events ────────────────────────────────────────────────────────────
@app.get("/v1/events")
async def list_events(
    limit:    int = Query(100,  ge=1, le=1000),
    kind:     Optional[int] = None,
    severity: Optional[int] = None,
    since:    Optional[int] = None,   # ms epoch
):
    clauses, params = ["1=1"], []
    if kind     is not None: clauses.append("kind=?");      params.append(kind)
    if severity is not None: clauses.append("severity=?");  params.append(severity)
    if since    is not None: clauses.append("received_at>?"); params.append(since)
    params.append(limit)

    async with aiosqlite.connect(DB_PATH) as db:
        cur = await db.execute(
            f"SELECT id,received_at,client_id,session_id,game_id,"
            f"timestamp_utc,severity,kind,detector_ver,evidence_json "
            f"FROM events WHERE {' AND '.join(clauses)} "
            f"ORDER BY received_at DESC LIMIT ?",
            params
        )
        rows = await cur.fetchall()
    return [row_to_dict(r) for r in rows]

# ── DELETE /v1/events ─────────────────────────────────────────────────────────
@app.delete("/v1/events", status_code=204, dependencies=[Depends(require_api_key)])
async def clear_events():
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("DELETE FROM events")
        await db.commit()
    return JSONResponse(None, status_code=204)

# ── GET /metrics ──────────────────────────────────────────────────────────────
@app.get("/metrics")
async def metrics():
    async with aiosqlite.connect(DB_PATH) as db:
        total   = (await (await db.execute("SELECT COUNT(*) FROM events")).fetchone())[0]
        by_kind = await (await db.execute(
            "SELECT kind, COUNT(*) FROM events GROUP BY kind")).fetchall()
        by_sev  = await (await db.execute(
            "SELECT severity, COUNT(*) FROM events GROUP BY severity")).fetchall()
    return {
        "total_events": total,
        "by_kind":     {KIND_MAP.get(r[0], str(r[0])): r[1] for r in by_kind},
        "by_severity": {SEVERITY_MAP.get(r[0], str(r[0])): r[1] for r in by_sev},
    }

# ── GET /dashboard ────────────────────────────────────────────────────────────
@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard(request: Request):
    return TEMPLATES.TemplateResponse("dashboard.html", {"request": request})
