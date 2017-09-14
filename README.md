# myrime

ibus-rime: Rime Input Method Engine for Linux/IBus

build dependencies:
  - pkg-config
  - cmake>=2.8
  - librime>=1.0 (development package)
  - libibus-1.0 (development package)
  - libnotify (development package)

runtime dependencies:
  - brise>=0.30
  - ibus
  - librime>=1.0
  - libibus-1.0
  - libnotify

RIME: Rime Input Method Engine

build dependencies:
  - cmake>=2.8
  - libboost>=1.46
  - libglog
  - libkyotocabinet
  - libopencc
  - libyaml-cpp>=0.5
  - libgtest (optional)

runtime dependencies:
  - libboost
  - libglog
  - libkyotocabinet
  - libopencc
  - libyaml-cpp

 
How to build 
=============

```$ cd ibus-rime```

```$ install.sh```

The install.sh will run make for ibus-rime, brise, and librime.

If successfully, related binary executable and object files will be installed in correct paths then. 
