#!/usr/bin/env python3
"""
Multiprocessing clang-format script for C++ files.
Formats all .hpp and .cpp files in include/ and src/ directories.
"""

import os
import glob
import subprocess
import multiprocessing
from pathlib import Path
from typing import List, Tuple
import sys


def format_file(file_path: str) -> tuple[str, bool, str]:
    """
    Format a single file with clang-format.

    Args:
        file_path: Path to the file to format

    Returns:
        Tuple of (file_path, success, error_message)
    """
    try:
        _ = subprocess.run(
            ['clang-format', '--style=file', '-i', file_path],
            capture_output=True,
            text=True,
            check=True
        )
        return (file_path, True, "")
    except subprocess.CalledProcessError as e:
        error_msg = f"Error: {e.stderr.strip()}" if e.stderr else f"Return code: {e.returncode}"
        return (file_path, False, error_msg)
    except FileNotFoundError:
        return (file_path, False, "clang-format not found in PATH")
    except Exception as e:
        return (file_path, False, str(e))


def find_cpp_files(directories: List[str]) -> List[str]:
    """
    Find all .hpp and .cpp files in the specified directories.

    Args:
        directories: List of directory paths to search

    Returns:
        List of file paths
    """
    cpp_files: List[str] = []
    extensions = ['*.hpp', '*.cpp']

    for directory in directories:
        if not os.path.exists(directory):
            print(
                f"Warning: Directory '{directory}' does not exist, skipping...")
            continue

        for extension in extensions:
            pattern = os.path.join(directory, '**', extension)
            files = glob.glob(pattern, recursive=True)
            cpp_files.extend(files)

    return sorted(cpp_files)


def check_clang_format_config() -> bool:
    """
    Check if .clang-format file exists in current directory or parent directories.

    Returns:
        True if config file is found, False otherwise
    """
    current_dir = Path.cwd()

    # Check current directory and parent directories
    for parent in [current_dir] + list(current_dir.parents):
        config_file = parent / '.clang-format'
        if config_file.exists():
            print(f"Found .clang-format config at: {config_file}")
            return True

    print("Warning: No .clang-format file found. clang-format will use default style.")
    return False


def main():
    """Main function to orchestrate the formatting process."""
    print("C++ Code Formatter with clang-format")
    print("=" * 40)

    # Check for clang-format availability
    try:
        subprocess.run(['clang-format', '--version'],
                       capture_output=True, check=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Error: clang-format not found in PATH")
        print("Please install clang-format or ensure it's in your PATH")
        sys.exit(1)

    # Check for .clang-format config file
    check_clang_format_config()

    additional_directories = ['lib/sv/include', 'lib/sv/src']

    # Find all C++ files
    directories = ['include', 'src']
    directories += additional_directories
    cpp_files = find_cpp_files(directories)

    if not cpp_files:
        print("No .hpp or .cpp files found in include/ and src/ directories.")
        return

    print(f"Found {len(cpp_files)} C++ files to format:")
    for file_path in cpp_files:
        print(f"  {file_path}")

    # Get number of CPU cores for multiprocessing
    num_processes = multiprocessing.cpu_count()
    print(f"\nUsing {num_processes} processes for formatting...")

    # Format files using multiprocessing
    with multiprocessing.Pool(processes=num_processes) as pool:
        results = pool.map(format_file, cpp_files)

    # Process results
    successful_files: List[str] = []
    failed_files: List[Tuple[str, str]] = []

    for file_path, success, error_msg in results:
        if success:
            successful_files.append(file_path)
        else:
            failed_files.append((file_path, error_msg))

    # Print summary
    print("\n" + "=" * 40)
    print("FORMATTING SUMMARY")
    print("=" * 40)

    if successful_files:
        print(f"✅ Successfully formatted {len(successful_files)} files:")
        for file_path in successful_files:
            print(f"  ✅ {file_path}")

    if failed_files:
        print(f"\n❌ Failed to format {len(failed_files)} files:")
        for file_path, error_msg in failed_files:
            print(f"  ❌ {file_path}: {error_msg}")

    print(
        f"\nTotal: {len(successful_files)} successful, {len(failed_files)} failed")

    # Exit with error code if any files failed
    if failed_files:
        sys.exit(1)


if __name__ == "__main__":
    main()
