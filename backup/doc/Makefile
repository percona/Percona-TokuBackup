.PHONY: FORCE_MAKE
all: hotbackup.pdf
master-preamble.isy: master-ipe.tex
	ipescript update-master master-ipe.tex figures/*.pdf
%.pdf : %.tex master-preamble.isy FORCE_MAKE
	latexmk -pdf -dvi- -ps- $<
