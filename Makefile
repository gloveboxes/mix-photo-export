IMAGE ?= mix-photo-export:local
INPUT ?=
OUTPUT ?=
QUALITY ?= 95

.PHONY: build test docker-build docker-test docker-convert apple-build apple-test apple-convert

build:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j4

test: build
	ctest --test-dir build --output-on-failure

docker-build:
	MIX_IMAGE="$(IMAGE)" bash scripts/mix-photo-export --runtime docker build

docker-test:
	MIX_IMAGE="$(IMAGE)" bash scripts/mix-photo-export --runtime docker test

docker-convert:
	MIX_IMAGE="$(IMAGE)" bash scripts/mix-photo-export --runtime docker convert "$(INPUT)" "$(OUTPUT)" --quality "$(QUALITY)"

apple-build:
	MIX_IMAGE="$(IMAGE)" bash scripts/mix-photo-export --runtime apple build

apple-test:
	MIX_IMAGE="$(IMAGE)" bash scripts/mix-photo-export --runtime apple test

apple-convert:
	MIX_IMAGE="$(IMAGE)" bash scripts/mix-photo-export --runtime apple convert "$(INPUT)" "$(OUTPUT)" --quality "$(QUALITY)"