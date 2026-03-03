"""Quick test script to verify TCP connection to UnrealAgent plugin."""

import asyncio
import json
import sys


async def test():
    host = "127.0.0.1"
    port = 55557

    print(f"Connecting to UnrealAgent at {host}:{port}...")

    try:
        reader, writer = await asyncio.open_connection(host, port)
    except ConnectionRefusedError:
        print("ERROR: Connection refused. Is Unreal Editor running with UnrealAgent plugin?")
        sys.exit(1)

    print("Connected!")

    # Test requests
    tests = [
        ("list_tools", {}),
        ("get_project_info", {}),
        ("get_editor_state", {}),
    ]

    for method, params in tests:
        print(f"\n--- {method} ---")

        request = json.dumps({
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": tests.index((method, params)) + 1,
        }).encode("utf-8")

        header = f"Content-Length: {len(request)}\r\n\r\n".encode()
        writer.write(header + request)
        await writer.drain()

        # Read response
        content_length = None
        while True:
            line = await reader.readline()
            line_str = line.decode().strip()
            if line_str == "":
                break
            if line_str.lower().startswith("content-length:"):
                content_length = int(line_str.split(":")[1].strip())

        if content_length:
            response_data = await reader.readexactly(content_length)
            response = json.loads(response_data)
            print(json.dumps(response, indent=2, ensure_ascii=False))
        else:
            print("ERROR: No Content-Length in response")

    writer.close()
    await writer.wait_closed()
    print("\nDone!")


if __name__ == "__main__":
    asyncio.run(test())
