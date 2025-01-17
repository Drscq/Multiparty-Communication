#!/usr/bin/env python3
import sys
import subprocess

def install_zmq_cpp():
    """
    Installs the C++ version of ZeroMQ and cppzmq headers.
    """
    try:
        # Check the operating system
        print("Detecting operating system...")
        if sys.platform.startswith("linux"):
            print("Linux detected. Installing ZeroMQ for C++...")
            # Use apt for Debian-based systems
            subprocess.check_call(["sudo", "apt", "update"])
            subprocess.check_call(["sudo", "apt", "install", "-y", "libzmq3-dev", "cmake", "g++", "git"])

            print("Installing cppzmq from source...")
            subprocess.check_call(["git", "clone", "https://github.com/zeromq/cppzmq.git"])
            subprocess.check_call(["sudo", "cp", "cppzmq/zmq.hpp", "cppzmq/zmq_addon.hpp", "/usr/local/include/"])
            subprocess.check_call(["rm", "-rf", "cppzmq"])

        elif sys.platform.startswith("darwin"):
            print("macOS detected. Installing ZeroMQ for C++...")
            # Use Homebrew for macOS
            subprocess.check_call(["brew", "install", "zeromq", "cppzmq"])
        elif sys.platform.startswith("win32"):
            print("Windows detected. Manual installation required.")
            print("Please visit: https://github.com/zeromq/libzmq/releases to download and install the Windows binaries.")
        else:
            print("Unsupported operating system. Please install ZeroMQ manually.")
            sys.exit(1)
        
        print("ZeroMQ for C++ has been successfully installed!")
    except subprocess.CalledProcessError as e:
        print(f"An error occurred during the installation process: {e}")
        sys.exit(1)

if __name__ == "__main__":
    install_zmq_cpp()


