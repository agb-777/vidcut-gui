"""Convenience launcher (also the entry point for PyInstaller): python run_vidcut.py"""
import sys

from vidcut.app import main

if __name__ == "__main__":
    sys.exit(main())
