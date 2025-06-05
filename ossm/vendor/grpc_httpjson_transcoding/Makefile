CPP_PROTO_FILES = $(shell find . -type f \
		-regex "./\(src\|api\)/.*[.]\(h\|cc\|proto\)" \
		-not -path "./vendor/*")

.PHONY: build
build: clang-format
	bazelisk build //...

.PHONY: test
test: clang-format
	bazelisk test //...

.PHONY: clang-format
clang-format:
	@echo "--> formatting code with 'clang-format' tool"
	@echo $(CPP_PROTO_FILES) | xargs clang-format-14 -i
