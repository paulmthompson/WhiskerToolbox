#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "1. Running Doxygen..."
# This generates the XML in docs/api/xml
doxygen Doxyfile

echo "2. Running Doxybook2..."
# Ensure the output directory exists
mkdir -p docs/api_generated

# Parse the XML and output standard Markdown for Quarto
doxybook2 --input docs/api/xml --output docs/api_generated --config doxybook_config.json

echo "3. Rendering Quarto Website..."
# Render the final HTML site
quarto render docs/

echo "Documentation build complete! You can preview it by running 'quarto preview docs/'"