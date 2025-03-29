#!/usr/bin/env python3

import json
import os
import sys
import re
import shutil
from urllib.parse import urlparse

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

def extract_build_number(version_string):
    """Extract build number from version string like 'V5.0-123-branch'"""
    match = re.search(r'V\d+\.\d+-(\d+)', version_string)
    if match:
        return int(match.group(1))
    return 0  # Default to 0 if no build number found

def get_version_limit(branch):
    """Get maximum number of versions to keep based on branch"""
    if branch == 'stable':
        return 4
    elif branch == 'testing':
        return 4
    else:  # develop or any other branch
        return 10

def extract_directory_from_url(url):
    """Extract the directory path from a URL"""
    parsed_url = urlparse(url)
    path = parsed_url.path
    # Remove the filename from the path
    directory = os.path.dirname(path)
    return directory

def add_or_update_entry(data, soc, type_, branch, fw_version, url):
    # First check if entry already exists and update it if it does
    entry_exists = False
    for i, entry in enumerate(data['firmware']):
        if (entry['soc'] == soc and
            entry['type'] == type_ and
            entry['branch'] == branch and
            entry['fw_version'] == fw_version):
            # Update existing entry with same version
            data['firmware'][i] = {
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
            entry_exists = True
            break
    
    # If exact entry doesn't exist, add it and manage version limits
    if not entry_exists:
        # Get all entries for this soc/type/branch
        related_entries = [entry for entry in data['firmware'] 
                          if entry['soc'] == soc and 
                             entry['type'] == type_ and 
                             entry['branch'] == branch]
        
        # Sort by build number (highest first)
        related_entries.sort(
            key=lambda e: extract_build_number(e['fw_version']), 
            reverse=True
        )
        
        # Get the version limit for this branch
        version_limit = get_version_limit(branch)
        
        # If we're at the limit, remove oldest entries
        if len(related_entries) >= version_limit:
            # Keep track of entries to remove
            entries_to_remove = related_entries[version_limit-1:]
            
            # Remove old entries from data['firmware'] and their directories
            for entry_to_remove in entries_to_remove:
                delete_entry(data, soc, type_, branch, entry_to_remove['fw_version'], delete_files=True)
        
        # Add the new entry
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
        data['firmware'].append(new_entry)
    
    # Update the top-level rom entry for backward compatibility
    if soc == 'esp8266' and type_ == 'release' and branch == 'testing':
        data['rom'] = {
            "fw_version": fw_version,
            "url": url
        }
        
    return data

def delete_entry(data, soc, type_, branch, fw_version=None, delete_files=False):
    entries_to_delete = []
    
    # Find entries to delete
    if fw_version:
        # Delete specific version
        entries_to_delete = [entry for entry in data['firmware'] 
                            if (entry['soc'] == soc and 
                                entry['type'] == type_ and 
                                entry['branch'] == branch and 
                                entry['fw_version'] == fw_version)]
        data['firmware'] = [entry for entry in data['firmware'] 
                           if not (entry['soc'] == soc and 
                                  entry['type'] == type_ and 
                                  entry['branch'] == branch and 
                                  entry['fw_version'] == fw_version)]
    else:
        # Delete all versions for this combination
        entries_to_delete = [entry for entry in data['firmware'] 
                            if (entry['soc'] == soc and 
                                entry['type'] == type_ and 
                                entry['branch'] == branch)]
        data['firmware'] = [entry for entry in data['firmware'] 
                           if not (entry['soc'] == soc and 
                                  entry['type'] == type_ and 
                                  entry['branch'] == branch)]
    
    # Delete corresponding directories if requested
    if delete_files:
        for entry in entries_to_delete:
            try:
                url = entry['files']['rom']['url']
                directory = extract_directory_from_url(url)
                local_dir = os.path.join('.', directory.lstrip('/'))
                
                # Get version directory (one level up from SOC/type)
                version_dir = os.path.dirname(os.path.dirname(local_dir))
                
                print(f"Deleting directory: {version_dir}")
                if os.path.exists(version_dir):
                    shutil.rmtree(version_dir)
                else:
                    print(f"Directory not found: {version_dir}")
            except Exception as e:
                print(f"Error deleting directory: {str(e)}")
    
    return data

def list_entries(data, soc=None, type_=None, branch=None):
    filtered_entries = data['firmware']
    
    if soc:
        filtered_entries = [e for e in filtered_entries if e['soc'] == soc]
    if type_:
        filtered_entries = [e for e in filtered_entries if e['type'] == type_]
    if branch:
        filtered_entries = [e for e in filtered_entries if e['branch'] == branch]
    
    return filtered_entries

def main():
    if len(sys.argv) < 2:
        print("Usage: script.py <json_file> [add|delete|list] [arguments]")
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
            print("Usage: script.py <json_file> delete <soc> <type> <branch> [fw_version] [--keep-files]")
            return
        soc = sys.argv[3]
        type_ = sys.argv[4]
        branch = sys.argv[5]
        fw_version = None
        delete_files = True  # Default to deleting files
        
        # Check for optional arguments
        for i in range(6, len(sys.argv)):
            if sys.argv[i] == "--keep-files":
                delete_files = False
            elif not fw_version:  # First non-flag argument is fw_version
                fw_version = sys.argv[i]
        
        data = delete_entry(data, soc, type_, branch, fw_version, delete_files)
    elif action == 'list':
        if len(sys.argv) < 3:
            print("Usage: script.py <json_file> list [soc] [type] [branch]")
            return
        
        soc = sys.argv[3] if len(sys.argv) > 3 else None
        type_ = sys.argv[4] if len(sys.argv) > 4 else None
        branch = sys.argv[5] if len(sys.argv) > 5 else None
        
        entries = list_entries(data, soc, type_, branch)
        print(f"Found {len(entries)} entries:")
        for entry in sorted(entries, key=lambda e: (e['branch'], e['soc'], e['type'], extract_build_number(e['fw_version'])), reverse=True):
            print(f"{entry['soc']}/{entry['type']}/{entry['branch']}: {entry['fw_version']} - {entry['files']['rom']['url']}")
    else:
        print("Invalid action. Use add, delete, or list.")
        return

    # Only save if we made changes
    if action in ['add', 'delete']:
        save_json(data, file_path)

if __name__ == "__main__":
    main()