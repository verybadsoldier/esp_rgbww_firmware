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
        "firmware": [],
        "history": []  # Add the history array to the base structure
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
    # Make sure history array exists
    if "history" not in data:
        data["history"] = []
    
    # Debug: Print the entry we're trying to add
    print(f"Adding/updating: {soc}/{type_}/{branch}: {fw_version}")
    print(f"Current firmware entries: {len(data['firmware'])}")
    
    # Normalize inputs to prevent case/whitespace issues
    soc = soc.strip().lower()
    type_ = type_.strip().lower()
    branch = branch.strip().lower()
    
    # First check if entry already exists and update it if it does
    entry_exists = False
    for i, entry in enumerate(data['firmware']):
        # Normalize existing entry data for comparison
        entry_soc = entry['soc'].strip().lower()
        entry_type = entry['type'].strip().lower()
        entry_branch = entry['branch'].strip().lower()
        
        # Debug: Print comparison details
        print(f"Comparing with: {entry_soc}/{entry_type}/{entry_branch}")
        
        if (entry_soc == soc and
            entry_type == type_ and
            entry_branch == branch):
            # Found matching entry
            print(f"Match found: {entry['soc']}/{entry['type']}/{entry['branch']}")
            
            # Keep original case for the entry we're updating
            original_soc = entry['soc']
            original_type = entry['type']
            original_branch = entry['branch']
            
            if entry['fw_version'] == fw_version:
                # Update existing entry with same version - no history change
                print(f"Same version, just updating URL")
                data['firmware'][i] = {
                    "soc": original_soc,
                    "type": original_type,
                    "branch": original_branch,
                    "fw_version": fw_version,
                    "files": {
                        "rom": {
                            "url": url
                        }
                    }
                }
            else:
                # Same combination but different version - move to history
                print(f"Moving {original_soc}/{original_type}/{original_branch}: {entry['fw_version']} to history")
                # Create a copy to avoid reference issues
                history_entry = dict(entry)
                data['history'].append(history_entry)
                
                # Replace with new version
                data['firmware'][i] = {
                    "soc": original_soc,
                    "type": original_type,
                    "branch": original_branch,
                    "fw_version": fw_version,
                    "files": {
                        "rom": {
                            "url": url
                        }
                    }
                }
            entry_exists = True
            break
    
    # If no matching soc/type/branch entry exists, add it
    if not entry_exists:
        print(f"No existing entry found, adding new one")
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
    
    # Debug: Final state
    print(f"Final firmware entries: {len(data['firmware'])}")
    print(f"Final history entries: {len(data['history'])}")
    
    return data

def delete_entry(data, soc, type_, branch, fw_version=None, delete_files=False):
    entries_to_delete = []
    
    # Find entries to delete from firmware array
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
                                  
        # Also remove from history if present
        history_entries_to_delete = [entry for entry in data.get('history', []) 
                                    if (entry['soc'] == soc and 
                                        entry['type'] == type_ and 
                                        entry['branch'] == branch and 
                                        entry['fw_version'] == fw_version)]
        if 'history' in data:
            data['history'] = [entry for entry in data['history'] 
                              if not (entry['soc'] == soc and 
                                      entry['type'] == type_ and 
                                      entry['branch'] == branch and 
                                      entry['fw_version'] == fw_version)]
        
        entries_to_delete.extend(history_entries_to_delete)
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
                                  
        # Also remove from history if present
        if 'history' in data:
            history_entries_to_delete = [entry for entry in data['history'] 
                                        if (entry['soc'] == soc and 
                                            entry['type'] == type_ and 
                                            entry['branch'] == branch)]
            data['history'] = [entry for entry in data['history'] 
                              if not (entry['soc'] == soc and 
                                      entry['type'] == type_ and 
                                      entry['branch'] == branch)]
            
            entries_to_delete.extend(history_entries_to_delete)
    
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

def list_entries(data, soc=None, type_=None, branch=None, include_history=False):
    # Get entries from firmware array
    filtered_entries = data['firmware']
    
    if soc:
        filtered_entries = [e for e in filtered_entries if e['soc'] == soc]
    if type_:
        filtered_entries = [e for e in filtered_entries if e['type'] == type_]
    if branch:
        filtered_entries = [e for e in filtered_entries if e['branch'] == branch]
    
    # Include history entries if requested
    if include_history and 'history' in data:
        history_entries = data['history']
        
        if soc:
            history_entries = [e for e in history_entries if e['soc'] == soc]
        if type_:
            history_entries = [e for e in history_entries if e['type'] == type_]
        if branch:
            history_entries = [e for e in history_entries if e['branch'] == branch]
            
        # Combine entries but mark history entries
        for entry in history_entries:
            entry['is_history'] = True
            
        for entry in filtered_entries:
            entry['is_history'] = False
            
        filtered_entries.extend(history_entries)
    
    return filtered_entries

def extract_complete_version_number(version_string):
    """Extract a sortable number from version string like 'V5.0-123-branch'"""
    # Extract major.minor version
    major_minor_match = re.search(r'V(\d+)\.(\d+)', version_string)
    if not major_minor_match:
        return 0  # Default for invalid format
    
    major = int(major_minor_match.group(1))
    minor = int(major_minor_match.group(2))
    
    # Extract build number
    build_match = re.search(r'V\d+\.\d+-(\d+)', version_string)
    build = int(build_match.group(1)) if build_match else 0
    
    # Combine as major*10000 + minor*1000 + build
    return major * 10000 + minor * 1000 + build

def cull_history(data, dry_run=False):
    """Limit the number of historical versions based on branch limits"""
    if "history" not in data or not data["history"]:
        print("No history entries to cull")
        return data
    
    # Group entries by soc+type+branch
    grouped_entries = {}
    for entry in data["history"]:
        key = (entry["soc"], entry["type"], entry["branch"])
        if key not in grouped_entries:
            grouped_entries[key] = []
        grouped_entries[key].append(entry)
    
    entries_to_remove = []
    
    # Process each group
    for (soc, type_, branch), entries in grouped_entries.items():
        # Sort by version (descending)
        sorted_entries = sorted(entries, 
                               key=lambda e: extract_complete_version_number(e["fw_version"]), 
                               reverse=True)
        
        # Get limit for this branch
        limit = get_version_limit(branch)
        
        # Keep only the top N entries
        if len(sorted_entries) > limit:
            entries_to_keep = sorted_entries[:limit]
            to_remove = sorted_entries[limit:]
            
            print(f"For {soc}/{type_}/{branch}: keeping {limit} entries, removing {len(to_remove)} entries")
            
            # Add entries to remove list
            entries_to_remove.extend(to_remove)
            
            if dry_run:
                for entry in to_remove:
                    print(f"Would remove: {entry['soc']}/{entry['type']}/{entry['branch']}: {entry['fw_version']}")
    
    if not entries_to_remove:
        print("No entries need to be culled")
        return data
    
    if not dry_run:
        # Remove entries from history
        for entry in entries_to_remove:
            # Delete files if they exist
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
        
        # Update history array to exclude removed entries
        data["history"] = [entry for entry in data["history"] 
                         if entry not in entries_to_remove]
    
    return data

def main():
    if len(sys.argv) < 2:
        print("Usage: script.py <json_file> [add|delete|list] [arguments]")
        return

    file_path = sys.argv[1]
    action = sys.argv[2]

    data = load_json(file_path)
    if data is None:
        data = prime_json(file_path)
        
    # Ensure history array exists in existing files
    if "history" not in data:
        data["history"] = []

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
            print("Usage: script.py <json_file> list [soc] [type] [branch] [--history]")
            return
        
        soc = None
        type_ = None
        branch = None
        include_history = False
        
        # Parse arguments
        for i in range(3, len(sys.argv)):
            if sys.argv[i] == "--history":
                include_history = True
            elif soc is None:
                soc = sys.argv[i]
            elif type_ is None:
                type_ = sys.argv[i]
            elif branch is None:
                branch = sys.argv[i]
        
        entries = list_entries(data, soc, type_, branch, include_history)
        print(f"Found {len(entries)} entries:")
        for entry in sorted(entries, key=lambda e: (e['branch'], e['soc'], e['type'], extract_build_number(e['fw_version']), e.get('is_history', False)), reverse=True):
            history_marker = "[HISTORY] " if entry.get('is_history', False) else ""
            print(f"{history_marker}{entry['soc']}/{entry['type']}/{entry['branch']}: {entry['fw_version']} - {entry['files']['rom']['url']}")

    elif action == 'cull':
        # Parse arguments
        dry_run = False
        for i in range(3, len(sys.argv)):
            if sys.argv[i] == "--dry-run":
                dry_run = True
                
        print(f"Culling history entries (dry run: {dry_run})")
        data = cull_history(data, dry_run)
        
    else:
        print("Invalid action. Use add, delete, list, or cull.")
        return

    # Only save if we made changes and not in a dry run
    if action in ['add', 'delete'] or (action == 'cull' and not dry_run):
        save_json(data, file_path)


if __name__ == "__main__":
    main()