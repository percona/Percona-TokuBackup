.PHONY: FORCE_MAKE
all: hotbackup.pdf
%.pdf : %.tex FORCE_MAKE
	latexmk -pdf -dvi- -ps- $<
