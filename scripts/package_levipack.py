import os
import sys
import zipfile
import json
import argparse

def main():
    parser = argparse.ArgumentParser(description="Package LeviPack")
    parser.add_argument("--library", required=True, help="Path to compiled .so file")
    parser.add_argument("--output", required=True, help="Output .levipack path")
    args = parser.parse_args()

    if not os.path.exists(args.library):
        print(f"Error: Library file {args.library} not found.")
        sys.exit(1)

    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    manifest = {
        "format_version": 1,
        "header": {
            "name": "InGameKeyboard",
            "description": "In-game Virtual Keyboard Mod for LeviLauncher",
            "uuid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            "version": [1, 0, 0],
            "min_game_version": [1, 20, 0]
        },
        "modules": [
            {
                "type": "native",
                "uuid": "b2c3d4e5-f6a7-8901-bcde-f12345678901",
                "version": [1, 0, 0],
                "path": "libingame_keyboard.so"
            }
        ]
    }

    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as zipf:
        zipf.writestr("manifest.json", json.dumps(manifest, indent=2))
        zipf.write(args.library, arcname="libingame_keyboard.so")

    print(f"Successfully created LeviPack at: {args.output}")

if __name__ == "__main__":
    main()
  
