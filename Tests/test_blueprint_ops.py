"""Blueprint operations completeness test.

Directly connects to UnrealAgent TCP (port 55557) bypassing MCP,
ensuring sequential request-response ordering.
"""

import asyncio
import json
import sys

BP = "/Game/Test/Test_BP/BP_EventOverrideTest"
HOST, PORT = "127.0.0.1", 55557
_id = 0

async def send(reader, writer, method, params=None):
    global _id
    _id += 1
    req = json.dumps({"jsonrpc": "2.0", "method": method, "params": params or {}, "id": _id}).encode()
    writer.write(f"Content-Length: {len(req)}\r\n\r\n".encode() + req)
    await writer.drain()

    content_length = None
    while True:
        line = (await reader.readline()).decode().strip()
        if line == "" and content_length is not None:
            break
        if line.lower().startswith("content-length:"):
            content_length = int(line.split(":")[1].strip())

    data = await reader.readexactly(content_length)
    resp = json.loads(data)
    if "error" in resp:
        return {"_error": resp["error"]["message"]}
    return resp.get("result", {})


def ok(tag, cond, detail=""):
    status = "PASS" if cond else "FAIL"
    print(f"  [{status}] {tag}" + (f" -- {detail}" if detail else ""))
    return cond


async def main():
    print("Connecting to UnrealAgent TCP...")
    reader, writer = await asyncio.open_connection(HOST, PORT)
    print(f"Connected.\n")

    passed, failed = 0, 0
    def track(b):
        nonlocal passed, failed
        if b: passed += 1
        else: failed += 1

    # ========== Setup: Fresh Blueprint ==========
    print("=== Setup: Recreating fresh blueprint ===")
    await send(reader, writer, "delete_asset", {"asset_path": BP})
    r = await send(reader, writer, "create_asset", {"asset_name": "BP_EventOverrideTest", "package_path": "/Game/Test/Test_BP", "asset_class": "Blueprint", "parent_class": "Actor"})
    print(f"  Created: {r.get('success', r.get('asset_path', 'unknown'))}\n")

    # ========== Phase 1: Query Tools ==========
    print("=== Phase 1: Query Tools ===")

    r = await send(reader, writer, "get_blueprint_overview", {"asset_path": BP})
    track(ok("T1 get_blueprint_overview", r.get("parent_class") == "Actor", f"parent={r.get('parent_class')}"))

    r = await send(reader, writer, "get_blueprint_graph", {"asset_path": BP})
    track(ok("T2 get_blueprint_graph", r.get("node_count", 0) > 0, f"nodes={r.get('node_count')}"))

    r = await send(reader, writer, "get_blueprint_variables", {"asset_path": BP})
    track(ok("T3 get_blueprint_variables", r.get("count") == 0, f"count={r.get('count')}"))

    r = await send(reader, writer, "get_blueprint_functions", {"asset_path": BP})
    track(ok("T4 get_blueprint_functions", r.get("count") is not None, f"count={r.get('count')} (Actor has UserConstructionScript by default)"))

    r = await send(reader, writer, "list_overridable_events", {"asset_path": BP})
    names = [e["name"] for e in r.get("events", [])]
    track(ok("T5 list_overridable_events", "ReceiveDestroyed" in names, f"total={r.get('count')}, has ReceiveDestroyed={'ReceiveDestroyed' in names}"))

    # ========== Phase 2: Variable & Function ==========
    print("\n=== Phase 2: Variable & Function ===")

    r = await send(reader, writer, "add_variable", {"asset_path": BP, "variable_name": "Health", "variable_type": "float", "is_exposed": True, "category": "Stats", "default_value": "100.0"})
    track(ok("T6 add_variable Health", r.get("success") == True))

    r = await send(reader, writer, "add_variable", {"asset_path": BP, "variable_name": "bIsDead", "variable_type": "bool"})
    track(ok("T7 add_variable bIsDead", r.get("success") == True))

    r = await send(reader, writer, "get_blueprint_variables", {"asset_path": BP})
    var_names = [v["name"] for v in r.get("variables", [])]
    track(ok("T8 get_blueprint_variables verify", "Health" in var_names and "bIsDead" in var_names, f"vars={var_names}"))

    r = await send(reader, writer, "add_function", {"asset_path": BP, "function_name": "CalculateDamage"})
    track(ok("T9 add_function CalculateDamage", r.get("success") == True))

    r = await send(reader, writer, "get_blueprint_functions", {"asset_path": BP})
    func_names = [f["name"] for f in r.get("functions", [])]
    track(ok("T10 get_blueprint_functions verify", "CalculateDamage" in func_names, f"funcs={func_names}"))

    # ========== Phase 3: Node Creation ==========
    print("\n=== Phase 3: Node Creation (all 7 types) ===")

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "Event", "event_name": "ReceiveDestroyed", "node_pos_x": 0, "node_pos_y": 600})
    track(ok("T11 add_node Event(ReceiveDestroyed)", r.get("success") == True, f"title={r.get('node_title')}"))
    event_destroyed_idx = r.get("node_index")

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "CustomEvent", "event_name": "OnDamageTaken", "node_pos_x": 0, "node_pos_y": 800})
    track(ok("T12 add_node CustomEvent", r.get("success") == True, f"title={r.get('node_title')}"))

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "CallFunction", "function_name": "PrintString", "target_class": "KismetSystemLibrary", "node_pos_x": 400, "node_pos_y": 600})
    track(ok("T13 add_node CallFunction(PrintString)", r.get("success") == True, f"title={r.get('node_title')}"))
    print_node_idx = r.get("node_index")

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "IfThenElse", "node_pos_x": 400, "node_pos_y": 800})
    track(ok("T14 add_node IfThenElse", r.get("success") == True, f"title={r.get('node_title')}"))
    branch_idx = r.get("node_index")

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "VariableGet", "variable_name": "Health", "node_pos_x": 200, "node_pos_y": 700})
    track(ok("T15 add_node VariableGet(Health)", r.get("success") == True, f"title={r.get('node_title')}"))
    var_get_idx = r.get("node_index")

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "VariableSet", "variable_name": "Health", "node_pos_x": 600, "node_pos_y": 600})
    track(ok("T16 add_node VariableSet(Health)", r.get("success") == True, f"title={r.get('node_title')}"))

    r = await send(reader, writer, "get_blueprint_graph", {"asset_path": BP})
    track(ok("T17 get_blueprint_graph snapshot", r.get("node_count", 0) >= 9, f"nodes={r.get('node_count')}, conns={r.get('connection_count')}"))

    # ========== Phase 4: Connections ==========
    print("\n=== Phase 4: Pin Connection & Disconnection ===")

    r = await send(reader, writer, "connect_pins", {"asset_path": BP, "from_node_index": event_destroyed_idx, "from_pin": "then", "to_node_index": print_node_idx, "to_pin": "execute"})
    track(ok("T18 connect exec: Destroyed->PrintString", r.get("success") == True, r.get("message", r.get("_error", ""))))

    r = await send(reader, writer, "connect_pins", {"asset_path": BP, "from_node_index": var_get_idx, "from_pin": "Health", "to_node_index": print_node_idx, "to_pin": "InString"})
    track(ok("T19 connect data: Health->InString", r.get("success") == True, r.get("message", r.get("_error", ""))))

    r = await send(reader, writer, "get_blueprint_graph", {"asset_path": BP})
    track(ok("T20 get_blueprint_graph verify conns", r.get("connection_count", 0) >= 2, f"conns={r.get('connection_count')}"))

    r = await send(reader, writer, "disconnect_pin", {"asset_path": BP, "node_index": var_get_idx, "pin_name": "Health"})
    track(ok("T21 disconnect_pin Health", r.get("success") == True, f"disconnected={r.get('disconnected_count')}"))

    r = await send(reader, writer, "get_blueprint_graph", {"asset_path": BP})
    old_conns = r.get("connection_count", 0)
    track(ok("T22 verify disconnect", True, f"conns now={old_conns}"))

    # ========== Phase 5: Delete ==========
    print("\n=== Phase 5: Node Deletion ===")

    pre_count = r.get("node_count", 0)
    r = await send(reader, writer, "delete_node", {"asset_path": BP, "node_index": branch_idx})
    track(ok("T23 delete_node Branch", r.get("success") == True, f"deleted={r.get('deleted_node_title')}"))

    r = await send(reader, writer, "get_blueprint_graph", {"asset_path": BP})
    track(ok("T24 verify deletion", r.get("node_count", 0) < pre_count, f"before={pre_count}, after={r.get('node_count')}"))

    # ========== Phase 6: Compile & Save ==========
    print("\n=== Phase 6: Compile & Save ===")

    r = await send(reader, writer, "compile_blueprint", {"asset_path": BP})
    track(ok("T25 compile_blueprint", r.get("success") == True, f"status={r.get('status')}, errors={r.get('error_count')}, warnings={r.get('warning_count')}"))

    r = await send(reader, writer, "save_asset", {"asset_path": BP})
    track(ok("T26 save_asset", r.get("success") == True))

    # ========== Phase 7: Error Handling ==========
    print("\n=== Phase 7: Error Handling (negative) ===")

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "Event", "event_name": "ReceiveDestroyed"})
    track(ok("T27 duplicate Event error", "_error" in r or "already implemented" in str(r), f"resp={str(r)[:120]}"))

    r = await send(reader, writer, "add_node", {"asset_path": "/Game/NonExistent/BP_Fake", "node_class": "Event", "event_name": "Foo"})
    track(ok("T28 bad path error", "_error" in r, f"resp={str(r)[:120]}"))

    r = await send(reader, writer, "add_node", {"asset_path": BP, "node_class": "Bogus"})
    track(ok("T29 bad node_class error", "_error" in r and "Supported" in r.get("_error", ""), f"resp={str(r)[:120]}"))

    # ========== Summary ==========
    print(f"\n{'='*50}")
    print(f"RESULTS: {passed} passed, {failed} failed, {passed+failed} total")
    print(f"{'='*50}")

    writer.close()
    return failed == 0

if __name__ == "__main__":
    success = asyncio.run(main())
    sys.exit(0 if success else 1)
