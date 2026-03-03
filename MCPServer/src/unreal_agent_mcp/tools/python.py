"""Python execution tools — universal execution layer."""

import json
import logging
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

from ..server import mcp, connection

logger = logging.getLogger(__name__)

# Execution log file — append-only JSONL for Phase 2 pattern recognition
_LOG_DIR = Path(__file__).resolve().parent.parent.parent.parent / "Cache"
_LOG_FILE = _LOG_DIR / "execution_log.jsonl"


def _write_log_entry(entry: dict) -> None:
    """Append a single JSON line to the execution log."""
    try:
        _LOG_DIR.mkdir(parents=True, exist_ok=True)
        with open(_LOG_FILE, "a", encoding="utf-8") as f:
            f.write(json.dumps(entry, ensure_ascii=False) + "\n")
    except Exception as e:
        logger.warning(f"Failed to write execution log: {e}")


@mcp.tool()
async def execute_python(code: str) -> dict:
    """Execute Python code in the Unreal Editor context.

    Has access to the full 'unreal' module API. Use `import unreal` to get started.
    Context is stateful — variables, imports, and function definitions persist across calls.
    Use print() to produce output.

    Args:
        code: Python code to execute. Can be multi-line.
              Example: "import unreal\\nprint(unreal.EditorLevelLibrary.get_all_level_actors())"
    """
    start_time = time.monotonic()
    result = await connection.send_request("execute_python", {"code": code})
    elapsed_ms = (time.monotonic() - start_time) * 1000

    # Log execution for Phase 2 pattern recognition
    _write_log_entry({
        "id": str(uuid.uuid4()),
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "code": code,
        "result": {
            "success": result.get("success", False),
            "output": result.get("output", ""),
            "error": result.get("error"),
            "execution_ms": round(elapsed_ms, 1),
        },
    })

    return result


@mcp.tool()
async def reset_python_context() -> dict:
    """Reset the shared Python execution context.

    Clears all variables, imports, and function definitions.
    Use this when you want a clean slate.
    """
    return await connection.send_request("reset_python_context", {})
