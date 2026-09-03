# HerculesAC Backend Contract

This document defines the contract between the anti-cheat client
(`HerculesAC.exe` / `GameMon.exe`) and the backend event ingestion service.

---

## 1. Endpoint

```
POST /v1/events
```

**Content-Type:** `application/json`

**Body:** JSON array of one or more `AntiCheatEvent` objects (see §3).

**Headers:**

| Header | Value |
|---|---|
| `Content-Type` | `application/json` |
| `X-HAC-Client-ID` | The `client_id` field of the first event in the batch |

**Authentication:** The backend should validate `X-HAC-Client-ID` against its
registered client registry. Future revisions will add mTLS client certificates;
the `reporting.cert_sha256` field in `hac.manifest` pins the server certificate.

---

## 2. HTTP Status Semantics

| Status | Client action |
|---|---|
| 2xx | Accepted — remove events from local queue |
| 400–499 | Drop without retry — the event is malformed or the client is unknown |
| 429 | Back off — honour `Retry-After` header if present; else treat as 5xx |
| 5xx, network error | Retry with exponential backoff: 1 s → 5 s → 30 s → 5 min → 30 min (cap) |

The client **never** re-sends events that received a 4xx response.

---

## 3. AntiCheatEvent JSON Schema

```json
{
  "client_id":        "string",
  "session_id":       "string (UUIDv4)",
  "timestamp_utc_ms": "integer (ms since Unix epoch)",
  "game_id":          "string",
  "severity":         "integer (1=Info 2=Warn 3=Critical)",
  "kind":             "integer (DetectionKind — see §4)",
  "detector_version": "string (e.g. \"proc-name@1.0.0\")",
  "evidence_json":    "string (JSON object, kind-specific — see §5)",
  "signature":        "string (HMAC-SHA256 hex — reserved, empty in v1)"
}
```

**Rate limits:** the client caps at 10 events per second sustained, burst 50.
The backend should enforce these limits server-side and return 429 on violation.

---

## 4. DetectionKind Values

| Value | Name | Description |
|---|---|---|
| 0 | Unknown | Unused |
| 1 | ProcName | Known-bad process found by name |
| 2 | WinName | Known-bad window class / title found |
| 3 | ModuleHash | Loaded module hash matches a known-bad signature |
| 4 | RpmRate | `ReadProcessMemory` called at suspicious rate |
| 5 | WpmTarget | `WriteProcessMemory` targeted at game process |
| 6 | HookIntegrity | API prologue patched (detected via CRC32 snapshot) |
| 7 | IatHook | IAT entry redirected outside the expected module |
| 8 | EatHook | EAT entry redirected |
| 9 | CodeSectionDiverge | `.text` section SHA-256 differs from disk baseline |
| 10 | ThreadInject | Thread created in game process by unknown module |
| 11 | KnownBadDriver | Driver module matches known-bad name / hash |

---

## 5. evidence_json Schema per DetectionKind

### ProcName (1)

```json
{ "image_name": "string", "pid": "integer", "detail": "string" }
```

### WinName (2)

```json
{ "class_name": "string", "title": "string", "hwnd": "string (hex)" }
```

### ModuleHash (3)

```json
{ "module_path": "string", "observed_sha256": "string (64 hex)", "pid": "integer" }
```

### RpmRate (4)

```json
{
  "caller_pid": "integer",
  "caller_image": "string",
  "rate_per_second": "number",
  "duration_ms": "integer"
}
```

### WpmTarget (5)

```json
{ "caller_pid": "integer", "caller_image": "string", "target_pid": "integer" }
```

### HookIntegrity (6)

```json
{
  "api_name": "string",
  "expected_crc32": "string (hex)",
  "observed_crc32": "string (hex)"
}
```

### IatHook (7) / EatHook (8)

```json
{
  "module": "string",
  "api_name": "string",
  "expected_va": "string (hex)",
  "observed_va": "string (hex)",
  "hooking_module": "string"
}
```

### CodeSectionDiverge (9)

```json
{
  "module": "string",
  "section": "string",
  "disk_sha256": "string",
  "memory_sha256": "string"
}
```

### ThreadInject (10)

```json
{
  "target_pid": "integer",
  "thread_id": "integer",
  "start_address": "string (hex)",
  "backing_module": "string"
}
```

### KnownBadDriver (11)

```json
{
  "driver_name": "string",
  "driver_path": "string",
  "observed_sha256": "string"
}
```

---

## 6. Offline Queue

Events are persisted to SQLite at `%LOCALAPPDATA%\HerculesAC\queue.db`
before transmission. The queue is circular with a 10 MB cap; oldest rows
are evicted when the cap is exceeded. The queue survives process restarts.

---

## 7. Certificate Pinning (Future)

When `reporting.cert_sha256` is present in `hac.manifest`, the client will
refuse TLS connections whose server certificate SHA-256 does not match.
Certificate rotation requires a manifest re-sign and re-publish.
