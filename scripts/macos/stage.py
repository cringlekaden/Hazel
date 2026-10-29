"""Xcode post-build resource staging through the canonical layout owner."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import hazel
hazel.stage(sys.argv[1])
