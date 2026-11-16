DOC      := dokumentace
SECTIONS := $(wildcard sections/*.tex)
LATEX    := pdflatex -interaction=nonstopmode -halt-on-error -file-line-error

.PHONY: all clean distclean watch view

all: $(DOC).pdf

# Rebuilds when the main file or any section changes. LaTeX is run a second time only when it
# asks for it (references, longtable widths, ...), and a failed build never leaves a stale PDF.
$(DOC).pdf: $(DOC).tex $(SECTIONS)
	$(LATEX) $(DOC).tex
	@if grep -q "Rerun to get\|Table widths have changed" $(DOC).log; then \
		echo "Second pass..."; $(LATEX) $(DOC).tex; \
	fi

.DELETE_ON_ERROR:

# Rebuild automatically on every save (needs latexmk)
watch:
	latexmk -pdf -pvc -interaction=nonstopmode $(DOC).tex

view: $(DOC).pdf
	@xdg-open $(DOC).pdf >/dev/null 2>&1 || open $(DOC).pdf

# Remove build leftovers but keep the PDF
clean:
	rm -f *.aux *.log *.out *.toc *.lof *.lot *.fls *.fdb_latexmk *.synctex.gz *.bbl *.blg

# Remove everything that was generated, including the PDF
distclean: clean
	rm -f $(DOC).pdf