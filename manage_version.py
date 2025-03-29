#!/usr/bin/env python3

import json
import os
import sys

def load_json(file_path):
    if not os.path.exists(file_path):
        return None
    with open(file_path, 'r') as file:
        try:
            return json.load(file)
        except json.JSONDecodeError:
            return None

def save_json(data, file_path):
    with open(file_path, 'w') as file:
        json.dump(data, file, indent=4)

def prime_json(file_path):
    data = {
        "rom": {
            "fw_version": "pj-1-656-g3119",
            "url": "http://lightinator.de/download/esp8266/develop/release/rom0.bin"
        },
        "spiffs": {
            "webapp_version": "0.3.3",
            "url": "http://rgbww.dronezone.de/testing/spiff_rom.bin"
        },
        "firmware": []
    }
    save_json(data, file_path)
    return data

def add_or_update_entry(data, soc, type_, branch, fw_version, url):
    new_entry = {
        "soc": soc,
        "type": type_,
        "branch": branch,
        "fw_version": fw_version,
        "files": {
            "rom": {
                "url": url
            }
        }
    }
    for i, entry in enumerate(data['firmware']):
        if (entry['soc'] == soc and
            entry['type'] == type_ and
            entry['branch'] == branch):
            data['firmware'][i] = new_entry
            if soc == 'esp8266' and type_ == 'release' and branch == 'develop':
                data['rom'] = new_entry['files']['rom']
            return data
    data['firmware'].append(new_entry)
    if soc == 'esp8266' and type_ == 'release' and branch == 'develop':
        data['rom'] = {
            "fw_version": fw_version,
            "url": url
        }
    return data

def delete_entry(data, soc, type_, branch):
    data['firmware'] = [entry for entry in data['firmware'] if not (
        entry['soc'] == soc and entry['type'] == type_ and entry['branch'] == branch)]
    return data

def main():
    if len(sys.argv) < 2:
        print("Usage: script.py <json_file> [add|delete] [soc type branch fw_version url|soc type branch]")
        return

    file_path = sys.argv[1]
    action = sys.argv[2]

    data = load_json(file_path)
    if data is None:
        data = prime_json(file_path)

    if action == 'add':
        if len(sys.argv) < 7:
            print("Usage: script.py <json_file> add <soc> <type> <branch> <fw_version> <url>")
            return
        soc = sys.argv[3]
        type_ = sys.argv[4]
        branch = sys.argv[5]
        fw_version = sys.argv[6]
        url = sys.argv[7]
        data = add_or_update_entry(data, soc, type_, branch, fw_version, url)
    elif action == 'delete':
        if len(sys.argv) < 6:
            print("Usage: script.py <json_file> delete <soc> <type> <branch>")
            return
        soc = sys.argv[3]
        type_ = sys.argv[4]
        branch = sys.argv[5]
        data = delete_entry(data, soc, type_, branch)
    else:
        print("Invalid action. Use add or delete.")
        return

    save_json(data, file_path)

if __name__ == "__main__":
    main()
