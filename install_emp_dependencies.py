#!/usr/bin/env python3

import subprocess
import os

def main():
    print("Step 1: Installing required dependencies...")
    # Install dependencies for emp-tool and the install script
    pkgs = [
        "build-essential",
        "cmake",
        "git",
        "libssl-dev",
        "wget",
        "python3"
    ]
    try:
        subprocess.check_call(["sudo", "apt-get", "update"])
        subprocess.check_call(["sudo", "apt-get", "install", "-y"] + pkgs)
    except subprocess.CalledProcessError as e:
        print(f"Error installing dependencies: {e}")
        return

    print("Step 2: Downloading emp-tool installation script...")
    # Download the emp-tool installation script
    try:
        subprocess.check_call(["wget", "-O", "install.py", "https://raw.githubusercontent.com/emp-toolkit/emp-readme/master/scripts/install.py"])
    except subprocess.CalledProcessError as e:
        print(f"Error downloading install.py: {e}")
        return

    print("Step 3: Running the emp-tool installation script...")
    # Run the emp-tool installation script
    try:
        # Add options as needed, such as --tool for emp-tool installation
        subprocess.check_call(["python3", "install.py", "--deps", "--tool"])
    except subprocess.CalledProcessError as e:
        print(f"Error running install.py: {e}")
        return

    print("emp-tool and dependencies installed successfully!")

if __name__ == "__main__":
    main()
