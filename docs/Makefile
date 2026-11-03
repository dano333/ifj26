all: dokumentace.pdf

dokumentace.pdf: dokumentace.tex
	pdflatex -interaction=nonstopmode dokumentace.tex

clean:
	rm -f *.aux *.log *.out *.pdf dokumentace.pdf