#!/bin/bash
# Two passes so the embedded .docx is built from the same essay.json as the page.
set -e
cd "$(dirname "$0")"
python3 build.py
python3 doc/prep_docx.py
node doc/build_docx.js
python3 build.py
