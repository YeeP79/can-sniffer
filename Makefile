.PHONY: test test-cpp test-py lint lint-cpp lint-py build flash monitor size clean

test: test-cpp test-py

test-cpp:
	pio test -e native

test-py:
	python -m pytest analysis/test_analyze.py -v

lint: lint-cpp lint-py

lint-cpp:
	pio check -e esp32dev --skip-packages

lint-py:
	ruff check analysis/
	ruff format --check analysis/

build:
	pio run -e esp32dev

size:
	./scripts/check_size.sh

flash:
	pio run -e esp32dev --target upload

monitor:
	pio device monitor

clean:
	pio run --target clean
	rm -rf .pytest_cache __pycache__ analysis/__pycache__ .coverage coverage.xml .ruff_cache
