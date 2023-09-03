# Project information
project = "Nope Foundry"
project_copyright = f"2023 {project}"
author = "Clément Bœsch"
version = "main"
release = version

# General configuration
extensions = ["myst_parser", "sphinxcontrib.mermaid"]
source_suffix = {".md": "markdown"}

# Automatically create anchors on titles following GitHub logic
myst_heading_anchors = 3

myst_url_schemes = dict(
    http=None,
    https=None,
    source=f"https://github.com/NopeFoundry/nope.gl/tree/{version}/" + "{{path}}",
)
