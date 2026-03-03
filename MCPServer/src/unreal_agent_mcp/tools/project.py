"""Project information tools."""

from ..server import mcp, connection


@mcp.tool()
async def get_project_info() -> dict:
    """Get detailed information about the current Unreal project.

    Returns project name, engine version, project directory,
    modules list, and enabled plugins.
    """
    return await connection.send_request("get_project_info", {})


@mcp.tool()
async def get_editor_state() -> dict:
    """Get the current Unreal Editor state.

    Returns the active level name, PIE (Play In Editor) status,
    and currently selected actors with their positions.
    """
    return await connection.send_request("get_editor_state", {})
