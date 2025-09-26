.PHONY: all
all: services.svg

%.svg: %.dot Makefile
	dot -Tsvg -o "$@" "$<"
