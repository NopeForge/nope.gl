import sys
from pathlib import Path

# Make it possible to import our nope extension
sys.path.append((Path(__file__).resolve().parent / "_ext").as_posix())

# Project information
project = "Nope Foundry"
project_copyright = f"2023 {project}"
author = "Clément Bœsch"
version = "main"
release = version

# General configuration
extensions = ["myst_parser", "sphinxcontrib.mermaid", "nope"]
source_suffix = {".md": "markdown"}

html_theme_options = {
    "code_font_size": "0.8em",
}

# Automatically create anchors on titles following GitHub logic
myst_heading_anchors = 3

myst_url_schemes = dict(
    http=None,
    https=None,
    source=f"https://github.com/NopeFoundry/nope.gl/tree/{version}/" + "{{path}}",
)
