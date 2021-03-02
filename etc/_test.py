#!/usr/bin/env python
import platform
import os

def main():
  if platform.system == 'Linux':
    if platform.platform.linux_distribution[0] == 'Ubuntu' or 'Debian':
      os.system('bash ingredients.sh')
    
if __name__ == "__main__":
  main()
