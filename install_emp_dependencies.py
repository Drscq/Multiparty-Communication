#!/usr/bin/env python3

import subprocess

def main():
    # List of required packages for emp-tool
    pkgs = [
        "build-essential",
        "cmake",
        "git",
        "libssl-dev"
    ]
    
    print("Updating system package index...")
    subprocess.check_call(["sudo", "apt-get", "update"])
    
    print("Installing dependencies: " + " ".join(pkgs))
    subprocess.check_call(["sudo", "apt-get", "install", "-y"] + pkgs)
    
    # Clone emp-tool from GitHub
    print("Cloning emp-tool repository...")
    subprocess.check_call(["git", "clone", "--recursive", "https://github.com/emp-toolkit/emp-tool.git"])
    
    # Build emp-tool
    print("Building emp-tool...")
    subprocess.check_call(["mkdir", "-p", "emp-tool/build"])
    subprocess.check_call(["cmake", ".."], cwd="emp-tool/build")
    subprocess.check_call(["make", "-j4"], cwd="emp-tool/build")
    
    print("emp-tool installed successfully.")

if __name__ == '__main__':
    main()
