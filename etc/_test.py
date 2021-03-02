#!/usr/bin/env python

import platform
import os

def main():
  if platform.system == 'Linux':
    os.system('bash ingredients.sh')
  
  
if __name__ == "__main__":
  main()
