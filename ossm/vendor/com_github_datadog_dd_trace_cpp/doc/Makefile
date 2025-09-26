.PHONY: all
all: ownership.svg

%.svg: %.dot
	dot -Grankdir=LR -Tsvg -o $@ $<
