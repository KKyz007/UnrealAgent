"""FastMCP server and shared connection instance."""

import os

from mcp.server.fastmcp import FastMCP

from .connection import UnrealConnection

# Create MCP server
mcp = FastMCP("UnrealAgent")

# Create shared connection (configured via env vars)
connection = UnrealConnection(
    host=os.environ.get("UNREAL_AGENT_HOST", "127.0.0.1"),
    port=int(os.environ.get("UNREAL_AGENT_PORT", "55557")),
)


# -- MCP Resources --
# These provide background context that AI clients can read on startup


@mcp.resource("unreal://project/info")
async def project_info_resource() -> str:
    """Current Unreal project information (name, engine version, modules, plugins)."""
    import json

    try:
        result = await connection.send_request("get_project_info", {})
        return json.dumps(result, indent=2)
    except Exception as e:
        return f"Error: {e}"


@mcp.resource("unreal://editor/state")
async def editor_state_resource() -> str:
    """Current editor state (active level, PIE status, selected actors)."""
    import json

    try:
        result = await connection.send_request("get_editor_state", {})
        return json.dumps(result, indent=2)
    except Exception as e:
        return f"Error: {e}"
