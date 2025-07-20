# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'ATS Mini'
copyright = '2025, ATS Mini Community'
author = 'ATS Mini Community'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
  "myst_parser",
  'swagger_plugin_for_sphinx',
]

myst_heading_anchors = 3
myst_enable_extensions = [
    "attrs_inline",
    "attrs_block",
]

templates_path = ['_templates']
exclude_patterns = []

source_suffix = {
    '.md': 'markdown',
}

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'furo'
html_static_path = ['_static']
